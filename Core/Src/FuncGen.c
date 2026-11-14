#include "FuncGen.h"
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "AD9850.h"
#include "I2C_LCD.h"
#include "Rotary_Sw.h"
#include "Flash_Store.h"
#include "UsbCmd.h"

#define FG_SWEEP_UPDATE_MS 5U    /* DDS update rate while sweeping */
#define FG_SWEEP_LCD_MS    500U  /* LCD refresh rate while sweeping */
#define FG_SAVE_DELAY_MS   3000U /* save settings this long after the last input */
#define FG_SET_VERSION     2U    /* bump when FG_Settings changes */

typedef enum { SCR_MAIN = 0, SCR_MENU } FG_Screen;

typedef enum
{
	MI_OUTPUT = 0,
	MI_PHASE,
	MI_BURST,
	MI_BU_ON,
	MI_BU_OFF,
	MI_SWEEP,
	MI_SW_START,
	MI_SW_STOP,
	MI_SW_TIME,
	MI_EXIT,
	MI_COUNT
} FG_MenuItem;

static const char *const k_menu_names[MI_COUNT] = {
	"Output", "Phase", "Burst", "On time", "Off time", "Sweep", "Sw start", "Sw stop", "Sw time", "Exit"};

typedef char fg_settings_size_check[(sizeof(FG_Settings) <= FS_MAX_PAYLOAD) ? 1 : -1];

static const FG_Settings k_defaults = {
	1000U,   /* freq */
	100U,    /* sw_start */
	10000U,  /* sw_stop */
	20U,     /* sw_time 2.0 s */
	10U,     /* bu_on_ms */
	90U,     /* bu_off_ms */
	0U,      /* phase */
	3U,      /* digit: 1 kHz */
	1U,      /* out_on */
	0U,      /* sweep_on */
	0U       /* burst_on */
};

static FG_Settings s_set;
static FG_Settings s_saved;           /* copy of what is in flash */
static uint8_t     s_save_pending = 0;
static uint32_t    s_last_input = 0;

static FG_Screen s_screen = SCR_MAIN;
static uint8_t   s_menu_idx = 0;
static uint8_t   s_menu_top = 0;      /* first menu item shown on row 1 */
static uint8_t   s_editing = 0;
static uint8_t   s_edit_digit = 3;
static uint8_t   s_dirty = 1;

static uint32_t  s_cur_freq = 0;      /* frequency currently on the output */
static uint32_t  s_sweep_t0 = 0;
static uint32_t  s_sweep_last = 0;
static uint32_t  s_lcd_last = 0;

/* Burst gate, run from SysTick (FG_Tick_1ms). The main loop changes these
 * only with interrupts masked. */
static volatile uint8_t  s_gate_enable = 0;
static volatile uint8_t  s_gate_open = 1;
static volatile uint16_t s_gate_on_ms = 0;
static volatile uint32_t s_gate_period = 0;
static volatile uint32_t s_gate_t = 0;
static volatile uint32_t s_dds_freq = 0;   /* last frequency/phase for the DDS */
static volatile uint8_t  s_dds_phase = 0;

static char      s_lcd_cache[LCD_ROWS][LCD_COLS + 1];
static uint8_t   s_cur_shown = 0;
static int8_t    s_cur_row = -1;
static int8_t    s_cur_col = -1;

/* ------------------------------------------------------------------------ */

static uint32_t pow10_u32(uint8_t n)
{
	uint32_t p = 1U;
	while (n--)
	{
		p *= 10U;
	}
	return p;
}

static uint32_t adjust_freq(uint32_t f, int16_t delta, uint8_t digit)
{
	int64_t v = (int64_t)f + (int64_t)delta * (int64_t)pow10_u32(digit);
	if (v < 0)
	{
		v = 0;
	}
	if (v > (int64_t)FG_MAX_FREQ)
	{
		v = FG_MAX_FREQ;
	}
	return (uint32_t)v;
}

/* 10 chars, right aligned, thousands separators: "40,000,000" / "     1,000" */
static void fmt_freq(char *out, uint32_t f)
{
	char tmp[11];
	int pos = 10;
	int d = 0;

	tmp[10] = '\0';
	do
	{
		if (d != 0 && (d % 3) == 0)
		{
			tmp[--pos] = ',';
		}
		tmp[--pos] = (char)('0' + (f % 10U));
		f /= 10U;
		d++;
	} while (f != 0U && pos > 0);
	while (pos > 0)
	{
		tmp[--pos] = ' ';
	}
	memcpy(out, tmp, sizeof(tmp));
}

/* LCD column of a digit inside a fmt_freq() field that starts at column 'field_col' */
static int8_t digit_col(uint8_t field_col, uint8_t digit)
{
	return (int8_t)(field_col + 9 - digit - (digit / 3));
}

static const char *const k_step_names[FG_MAX_DIGIT + 1U] = {
	"1 Hz", "10 Hz", "100 Hz", "1 kHz", "10 kHz", "100 kHz", "1 MHz", "10 MHz"};

/* Move a step digit by 'd' (press-and-turn), clamped to 1 Hz .. 10 MHz */
static uint8_t shift_digit(uint8_t digit, int16_t d)
{
	int32_t v = (int32_t)digit + d;
	if (v < 0)
	{
		v = 0;
	}
	if (v > (int32_t)FG_MAX_DIGIT)
	{
		v = FG_MAX_DIGIT;
	}
	return (uint8_t)v;
}

static uint8_t is_freq_item(uint8_t idx)
{
	return (idx == MI_SW_START) || (idx == MI_SW_STOP);
}

/* ------------------------------------------------------------------------ */

/* Reject records with out-of-range values (e.g. from an older layout) */
static uint8_t settings_valid(const FG_Settings *p)
{
	return (p->freq <= FG_MAX_FREQ) && (p->sw_start <= FG_MAX_FREQ) &&
	       (p->sw_stop <= FG_MAX_FREQ) &&
	       (p->sw_time_ds >= FG_SWT_MIN) && (p->sw_time_ds <= FG_SWT_MAX) &&
	       (p->phase < AD_PHASE_STEPS) && (p->digit <= FG_MAX_DIGIT) &&
	       (p->bu_on_ms >= FG_BURST_MIN_MS) && (p->bu_on_ms <= FG_BURST_MAX_MS) &&
	       (p->bu_off_ms >= FG_BURST_MIN_MS) && (p->bu_off_ms <= FG_BURST_MAX_MS) &&
	       (p->out_on <= 1U) && (p->sweep_on <= 1U) && (p->burst_on <= 1U);
}

/* Load settings from flash; defaults if none stored, invalid, or 'force_defaults' */
static void settings_load(uint8_t force_defaults)
{
	FG_Settings tmp;

	memcpy(&s_set, &k_defaults, sizeof(s_set));
	if (!force_defaults && FS_Load(&tmp, sizeof(tmp), FG_SET_VERSION) && settings_valid(&tmp))
	{
		memcpy(&s_set, &tmp, sizeof(s_set));
		memcpy(&s_saved, &s_set, sizeof(s_saved));
		return;
	}
	/* Nothing usable in flash (or reset requested): store defaults later */
	memset(&s_saved, 0xFF, sizeof(s_saved));
	s_save_pending = 1;
	s_last_input = HAL_GetTick();
}

/* Write settings once the user has stopped changing them for FG_SAVE_DELAY_MS */
static void settings_autosave(uint32_t now)
{
	if (!s_save_pending || (now - s_last_input) < FG_SAVE_DELAY_MS)
	{
		return;
	}
	s_save_pending = 0;
	if (memcmp(&s_set, &s_saved, sizeof(s_set)) == 0)
	{
		return;
	}
	if (FS_Save(&s_set, sizeof(s_set), FG_SET_VERSION) == HAL_OK)
	{
		memcpy(&s_saved, &s_set, sizeof(s_saved));
	}
}

/* ------------------------------------------------------------------------ */

static uint32_t irq_save(void)
{
	uint32_t m = __get_PRIMASK();
	__disable_irq();
	return m;
}

static void irq_restore(uint32_t m)
{
	__set_PRIMASK(m);
}

/* Send a frequency to the DDS. While the burst gate is closed the value is
 * only remembered; the gate sends it when it opens. */
static void dds_write(uint32_t freq)
{
	uint32_t m = irq_save();
	s_dds_freq = freq;
	s_dds_phase = s_set.phase;
	if (!s_gate_enable || s_gate_open)
	{
		AD_Set_Freq(freq, s_set.phase);
	}
	irq_restore(m);
}

static void fg_apply_output(void)
{
	uint32_t m = irq_save();
	s_gate_enable = (uint8_t)(s_set.out_on && s_set.burst_on);
	s_gate_on_ms = s_set.bu_on_ms;
	s_gate_period = (uint32_t)s_set.bu_on_ms + s_set.bu_off_ms;
	s_gate_t = 0;
	s_gate_open = 1; /* each burst cycle starts with the on time */
	if (!s_set.out_on)
	{
		AD_Power_Down();
	}
	irq_restore(m);

	if (!s_set.out_on)
	{
		return;
	}
	if (s_set.sweep_on)
	{
		s_sweep_t0 = HAL_GetTick();
		s_sweep_last = s_sweep_t0;
		s_cur_freq = s_set.sw_start;
	}
	else
	{
		s_cur_freq = s_set.freq;
	}
	dds_write(s_cur_freq);
}

/* Burst times: 1 ms steps below 100 ms, 10 ms below 1 s, 100 ms below 10 s, then 1 s */
static uint32_t burst_step(uint32_t v)
{
	return (v < 100U) ? 1U : (v < 1000U) ? 10U : (v < 10000U) ? 100U : 1000U;
}

static uint16_t adjust_burst(uint16_t v, int16_t d)
{
	int32_t x = v;
	while (d > 0)
	{
		x += (int32_t)burst_step((uint32_t)x);
		d--;
	}
	while (d < 0)
	{
		x -= (int32_t)burst_step((x > 1) ? (uint32_t)(x - 1) : 0U);
		d++;
	}
	if (x < (int32_t)FG_BURST_MIN_MS)
	{
		x = FG_BURST_MIN_MS;
	}
	if (x > (int32_t)FG_BURST_MAX_MS)
	{
		x = FG_BURST_MAX_MS;
	}
	return (uint16_t)x;
}

static void fg_sweep_task(void)
{
	uint32_t now, period, t;
	int64_t span;

	if (!s_set.sweep_on || !s_set.out_on)
	{
		return;
	}
	now = HAL_GetTick();
	if ((now - s_sweep_last) < FG_SWEEP_UPDATE_MS)
	{
		return;
	}
	s_sweep_last = now;

	period = (uint32_t)s_set.sw_time_ds * 100U;
	t = (now - s_sweep_t0) % period;
	span = (int64_t)s_set.sw_stop - (int64_t)s_set.sw_start; /* negative = sweep down */
	s_cur_freq = (uint32_t)((int64_t)s_set.sw_start + (span * (int64_t)t) / (int64_t)period);
	dds_write(s_cur_freq);
}

/* ------------------------------------------------------------------------ */

static void enter_menu(void)
{
	s_screen = SCR_MENU;
	s_menu_idx = 0;
	s_menu_top = 0;
	s_editing = 0;
	s_dirty = 1;
}

static void exit_menu(void)
{
	s_screen = SCR_MAIN;
	s_editing = 0;
	s_dirty = 1;
	fg_apply_output();
}

static void handle_main(int16_t d, int16_t hd, RS_BtnEvent b)
{
	if (b == RS_BTN_LONG)
	{
		enter_menu();
		return;
	}

	if (s_set.sweep_on)
	{
		if (b == RS_BTN_SHORT)
		{
			s_set.sweep_on = 0;
			fg_apply_output();
			s_dirty = 1;
		}
		return;
	}

	if (b == RS_BTN_SHORT)
	{
		s_set.digit = (uint8_t)((s_set.digit + 1U) % (FG_MAX_DIGIT + 1U));
		s_dirty = 1;
	}
	if (hd != 0)
	{
		s_set.digit = shift_digit(s_set.digit, hd);
		s_dirty = 1;
	}
	if (d != 0)
	{
		s_set.freq = adjust_freq(s_set.freq, d, s_set.digit);
		fg_apply_output();
		s_dirty = 1;
	}
}

static void edit_value(int16_t d)
{
	int32_t v;

	switch (s_menu_idx)
	{
	case MI_OUTPUT:
		if (d & 1)
		{
			s_set.out_on = (uint8_t)!s_set.out_on;
		}
		break;
	case MI_PHASE:
		v = ((int32_t)s_set.phase + d) % (int32_t)AD_PHASE_STEPS;
		if (v < 0)
		{
			v += AD_PHASE_STEPS;
		}
		s_set.phase = (uint8_t)v;
		break;
	case MI_BURST:
		if (d & 1)
		{
			s_set.burst_on = (uint8_t)!s_set.burst_on;
		}
		break;
	case MI_BU_ON:
		s_set.bu_on_ms = adjust_burst(s_set.bu_on_ms, d);
		break;
	case MI_BU_OFF:
		s_set.bu_off_ms = adjust_burst(s_set.bu_off_ms, d);
		break;
	case MI_SWEEP:
		if (d & 1)
		{
			s_set.sweep_on = (uint8_t)!s_set.sweep_on;
		}
		break;
	case MI_SW_START:
		s_set.sw_start = adjust_freq(s_set.sw_start, d, s_edit_digit);
		break;
	case MI_SW_STOP:
		s_set.sw_stop = adjust_freq(s_set.sw_stop, d, s_edit_digit);
		break;
	case MI_SW_TIME:
		v = (int32_t)s_set.sw_time_ds + d * ((s_set.sw_time_ds >= 100U) ? 10 : 1);
		if (v < (int32_t)FG_SWT_MIN)
		{
			v = FG_SWT_MIN;
		}
		if (v > (int32_t)FG_SWT_MAX)
		{
			v = FG_SWT_MAX;
		}
		s_set.sw_time_ds = (uint16_t)v;
		break;
	default:
		break;
	}
	fg_apply_output();
}

static void handle_menu(int16_t d, int16_t hd, RS_BtnEvent b)
{
	if (!s_editing)
	{
		if (b == RS_BTN_LONG)
		{
			exit_menu();
			return;
		}
		if (d != 0)
		{
			int32_t i = ((int32_t)s_menu_idx + d) % (int32_t)MI_COUNT;
			if (i < 0)
			{
				i += MI_COUNT;
			}
			s_menu_idx = (uint8_t)i;
			s_dirty = 1;
		}
		if (b == RS_BTN_SHORT)
		{
			if (s_menu_idx == MI_EXIT)
			{
				exit_menu();
				return;
			}
			s_editing = 1;
			s_edit_digit = 3;
			s_dirty = 1;
		}
		return;
	}

	/* editing */
	if (hd != 0 && is_freq_item(s_menu_idx))
	{
		s_edit_digit = shift_digit(s_edit_digit, hd);
		s_dirty = 1;
	}
	if (b == RS_BTN_LONG)
	{
		s_editing = 0;
		s_dirty = 1;
		return;
	}
	if (b == RS_BTN_SHORT)
	{
		if (is_freq_item(s_menu_idx))
		{
			s_edit_digit = (uint8_t)((s_edit_digit + 1U) % (FG_MAX_DIGIT + 1U));
		}
		else
		{
			s_editing = 0;
		}
		s_dirty = 1;
	}
	if (d != 0)
	{
		edit_value(d);
		s_dirty = 1;
	}
}

/* ------------------------------------------------------------------------ */

static uint8_t lcd_update_line(uint8_t row, const char *str)
{
	char line[LCD_COLS + 1];
	size_t len = strlen(str);
	if (len > LCD_COLS)
	{
		len = LCD_COLS;
	}
	memcpy(line, str, len);
	memset(&line[len], ' ', LCD_COLS - len);
	line[LCD_COLS] = '\0';

	if (strcmp(line, s_lcd_cache[row]) == 0)
	{
		return 0;
	}
	lcd_print_line(row, line);
	memcpy(s_lcd_cache[row], line, sizeof(line));
	return 1;
}

static void lcd_update_cursor(int8_t row, int8_t col, uint8_t moved)
{
	if (col < 0)
	{
		if (s_cur_shown)
		{
			lcd_cursor(0, 0);
			s_cur_shown = 0;
		}
		return;
	}
	if (moved || !s_cur_shown || row != s_cur_row || col != s_cur_col)
	{
		lcd_put_cur((uint8_t)row, (uint8_t)col);
		s_cur_row = row;
		s_cur_col = col;
	}
	if (!s_cur_shown)
	{
		lcd_cursor(1, 0);
		s_cur_shown = 1;
	}
}

#define MENU_ROWS   (LCD_ROWS - 1U)  /* rows 1..3 list menu items */
#define MAIN_FCOL   6                /* "Freq: " -> frequency field starts at column 6 */
#define MENU_FCOL   10               /* ">Sw start " -> value field starts at column 10 */

/* Right-aligned 10 char value of a menu item */
static void menu_value(uint8_t idx, char *out, size_t n)
{
	unsigned ph;
	switch (idx)
	{
	case MI_OUTPUT:
		snprintf(out, n, "%10s", s_set.out_on ? "ON" : "OFF");
		break;
	case MI_PHASE:
		ph = (unsigned)s_set.phase * 1125U; /* hundredths of a degree */
		snprintf(out, n, "   %3u.%02u\xDF", ph / 100U, ph % 100U);
		break;
	case MI_BURST:
		snprintf(out, n, "%10s", s_set.burst_on ? "ON" : "OFF");
		break;
	case MI_BU_ON:
		snprintf(out, n, "%7u ms", (unsigned)s_set.bu_on_ms);
		break;
	case MI_BU_OFF:
		snprintf(out, n, "%7u ms", (unsigned)s_set.bu_off_ms);
		break;
	case MI_SWEEP:
		snprintf(out, n, "%10s", s_set.sweep_on ? "ON" : "OFF");
		break;
	case MI_SW_START:
		fmt_freq(out, s_set.sw_start);
		break;
	case MI_SW_STOP:
		fmt_freq(out, s_set.sw_stop);
		break;
	case MI_SW_TIME:
		snprintf(out, n, "    %2u.%u s", (unsigned)(s_set.sw_time_ds / 10U), (unsigned)(s_set.sw_time_ds % 10U));
		break;
	default:
		out[0] = '\0';
		break;
	}
}

static void render(void)
{
	char l[LCD_ROWS][LCD_COLS + 12];
	char f[16];
	int8_t cur_row = -1;
	int8_t cur_col = -1;
	uint8_t moved = 0;
	uint8_t r;

	if (s_screen == SCR_MAIN)
	{
		fmt_freq(f, s_set.sweep_on ? s_cur_freq : s_set.freq);
		snprintf(l[0], sizeof(l[0]), "Freq: %s Hz", f);

		if (s_set.sweep_on)
		{
			fmt_freq(f, s_set.sw_start);
			snprintf(l[1], sizeof(l[1]), "Start:%s Hz", f);
			fmt_freq(f, s_set.sw_stop);
			snprintf(l[2], sizeof(l[2]), "Stop: %s Hz", f);
			snprintf(l[3], sizeof(l[3]), "Sweep %2u.%us  Out:%s",
			         (unsigned)(s_set.sw_time_ds / 10U), (unsigned)(s_set.sw_time_ds % 10U),
			         s_set.out_on ? (s_set.burst_on ? "BST" : "ON") : "OFF");
		}
		else
		{
			unsigned ph = (unsigned)s_set.phase * 1125U;
			snprintf(l[1], sizeof(l[1]), "Step: %s", k_step_names[s_set.digit]);
			snprintf(l[2], sizeof(l[2]), "Phase: %u.%02u\xDF", ph / 100U, ph % 100U);
			if (s_set.out_on && s_set.burst_on)
			{
				snprintf(l[3], sizeof(l[3]), "Burst %u/%u ms",
				         (unsigned)s_set.bu_on_ms, (unsigned)s_set.bu_off_ms);
			}
			else
			{
				snprintf(l[3], sizeof(l[3]), "Output: %s", s_set.out_on ? "ON" : "OFF");
			}
			cur_row = 0;
			cur_col = digit_col(MAIN_FCOL, s_set.digit);
		}
	}
	else
	{
		if (!s_editing)
		{
			snprintf(l[0], sizeof(l[0]), "MENU      hold=exit");
		}
		else if (is_freq_item(s_menu_idx))
		{
			snprintf(l[0], sizeof(l[0]), "EDIT Hz  press=digit");
		}
		else
		{
			snprintf(l[0], sizeof(l[0]), "EDIT      press=done");
		}

		/* keep the selected item inside the visible window */
		if (s_menu_idx < s_menu_top)
		{
			s_menu_top = s_menu_idx;
		}
		else if (s_menu_idx >= s_menu_top + MENU_ROWS)
		{
			s_menu_top = (uint8_t)(s_menu_idx - MENU_ROWS + 1U);
		}

		for (r = 0; r < MENU_ROWS; r++)
		{
			uint8_t idx = (uint8_t)(s_menu_top + r);
			if (idx >= MI_COUNT)
			{
				l[r + 1U][0] = '\0';
				continue;
			}
			menu_value(idx, f, sizeof(f));
			snprintf(l[r + 1U], sizeof(l[0]), "%c%-8s %s",
			         (idx == s_menu_idx) ? (s_editing ? '*' : '>') : ' ', k_menu_names[idx], f);
			if (idx == s_menu_idx && s_editing && is_freq_item(idx))
			{
				cur_row = (int8_t)(r + 1U);
				cur_col = digit_col(MENU_FCOL, s_edit_digit);
			}
		}
	}

	for (r = 0; r < LCD_ROWS; r++)
	{
		moved |= lcd_update_line(r, l[r]);
	}
	lcd_update_cursor(cur_row, cur_col, moved);
}

/* ------------------------------------------------------------------------ */

void FG_Init(I2C_HandleTypeDef *hi2c)
{
	AD_CFG ad_cfg;
	uint8_t reset;

	ad_cfg.W_CLK_PORT = W_CLK_GPIO_Port;
	ad_cfg.W_CLK_PIN  = W_CLK_Pin;
	ad_cfg.FU_FD_PORT = FU_UD_GPIO_Port;
	ad_cfg.FU_FD_PIN  = FU_UD_Pin;
	ad_cfg.DATA_PORT  = Data_GPIO_Port;
	ad_cfg.DATA_PIN   = Data_Pin;

	AD_Init(&ad_cfg);
	RS_Init();

	/* Holding the encoder button at power-up restores the default settings */
	reset = (HAL_GPIO_ReadPin(BTN_PUSH_GPIO_Port, BTN_PUSH_Pin) ==
	         (RS_BTN_ACTIVE_LEVEL ? GPIO_PIN_SET : GPIO_PIN_RESET));
	settings_load(reset);

	lcd_init(hi2c, SLAVE_ADDRESS_LCD);
	lcd_print_line(0, "  DDS Function Gen");
	lcd_print_line(1, "   AD9850 + STM32");
	lcd_print_line(2, "  0 Hz ... 40 MHz");
	lcd_print_line(3, reset ? " Defaults restored" : "  Sine / Square");
	HAL_Delay(reset ? 2000 : 1200);

	memset(s_lcd_cache, 0, sizeof(s_lcd_cache)); /* force full redraw */
	(void)RS_GetDelta();                         /* drop input made during splash */
	(void)RS_GetHeldDelta();
	(void)RS_GetButton();

	fg_apply_output();
	s_dirty = 1;
}

/* ---------------------------------------------- API for remote commands */

FG_Settings *FG_GetSettings(void)
{
	return &s_set;
}

void FG_SettingsChanged(void)
{
	fg_apply_output();
	s_dirty = 1;
	s_save_pending = 1;
	s_last_input = HAL_GetTick();
}

uint8_t FG_SaveNow(void)
{
	s_save_pending = 0;
	if (memcmp(&s_set, &s_saved, sizeof(s_set)) == 0)
	{
		return 1;
	}
	if (FS_Save(&s_set, sizeof(s_set), FG_SET_VERSION) == HAL_OK)
	{
		memcpy(&s_saved, &s_set, sizeof(s_saved));
		return 1;
	}
	return 0;
}

uint32_t FG_OutputFreq(void)
{
	return (s_set.sweep_on && s_set.out_on) ? s_cur_freq : s_set.freq;
}

void FG_Task(void)
{
	int16_t d = RS_GetDelta();
	int16_t hd = RS_GetHeldDelta();
	RS_BtnEvent b = RS_GetButton();
	uint32_t now;

	UC_Poll(); /* USB virtual COM port commands */

	if (d != 0 || hd != 0 || b != RS_BTN_NONE)
	{
		s_save_pending = 1;
		s_last_input = HAL_GetTick();
		if (s_screen == SCR_MAIN)
		{
			handle_main(d, hd, b);
		}
		else
		{
			handle_menu(d, hd, b);
		}
	}

	fg_sweep_task();

	now = HAL_GetTick();
	settings_autosave(now);

	if (s_set.sweep_on && s_set.out_on && s_screen == SCR_MAIN &&
	    (now - s_lcd_last) >= FG_SWEEP_LCD_MS)
	{
		s_dirty = 1;
	}

	if (s_dirty)
	{
		s_dirty = 0;
		s_lcd_last = now;
		render();
	}
}

/* Burst gate: call every 1 ms from SysTick_Handler */
void FG_Tick_1ms(void)
{
	uint8_t open;

	if (!s_gate_enable)
	{
		return;
	}
	if (++s_gate_t >= s_gate_period)
	{
		s_gate_t = 0;
	}
	open = (uint8_t)(s_gate_t < s_gate_on_ms);
	if (open != s_gate_open)
	{
		s_gate_open = open;
		if (open)
		{
			AD_Set_Freq(s_dds_freq, s_dds_phase);
		}
		else
		{
			AD_Power_Down();
		}
	}
}
