#include "UsbCmd.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "FuncGen.h"
#include "usbd_cdc_if.h"

#define UC_LINE_MAX    80U
#define UC_TX_MAX      512U
#define UC_TX_WAIT_MS  50U   /* give up sending if the PC does not read */

static char     s_line[UC_LINE_MAX];
static uint8_t  s_len = 0;
static uint8_t  s_overflow = 0;

static char     s_out[UC_TX_MAX];    /* reply being built */
static uint16_t s_out_len = 0;
static uint8_t  s_tx[UC_TX_MAX];     /* buffer owned by the USB transfer */

/* ----------------------------------------------------------------- output */

static void out(const char *fmt, ...)
{
	va_list ap;
	int n;

	if (s_out_len >= UC_TX_MAX - 1U)
	{
		return;
	}
	va_start(ap, fmt);
	n = vsnprintf(&s_out[s_out_len], UC_TX_MAX - s_out_len, fmt, ap);
	va_end(ap);
	if (n > 0)
	{
		s_out_len = (uint16_t)(s_out_len + (uint16_t)n);
		if (s_out_len > UC_TX_MAX - 1U)
		{
			s_out_len = UC_TX_MAX - 1U;
		}
	}
}

static void flush(void)
{
	uint32_t t0 = HAL_GetTick();

	if (s_out_len == 0U)
	{
		return;
	}
	if (!CDC_IsConnected())
	{
		s_out_len = 0;
		return;
	}
	while (CDC_TxBusy())
	{
		if ((HAL_GetTick() - t0) > UC_TX_WAIT_MS)
		{
			s_out_len = 0;   /* terminal not reading: drop the reply */
			return;
		}
	}
	memcpy(s_tx, s_out, s_out_len);
	(void)CDC_Transmit_FS(s_tx, s_out_len);
	s_out_len = 0;
}

/* ---------------------------------------------------------------- parsing */

static char *next_token(char **p)
{
	char *s = *p;
	char *t;

	while (*s == ' ' || *s == '\t' || *s == ',')
	{
		s++;
	}
	if (*s == '\0')
	{
		*p = s;
		return NULL;
	}
	t = s;
	while (*s != '\0' && *s != ' ' && *s != '\t' && *s != ',')
	{
		s++;
	}
	if (*s != '\0')
	{
		*s++ = '\0';
	}
	*p = s;
	return t;
}

/* Decimal number with up to 3 decimals, optional suffix K or M (x1e3, x1e6) and
 * optional unit HZ / S / MS / DEG. Result in thousandths. */
static uint8_t parse_milli(const char *s, uint64_t *milli)
{
	uint64_t ip = 0;
	uint64_t frac = 0;
	uint32_t fdig = 0;
	uint8_t digits = 0;
	uint64_t mult = 1;

	if (s == NULL)
	{
		return 0;
	}
	while (*s >= '0' && *s <= '9')
	{
		ip = ip * 10U + (uint64_t)(*s++ - '0');
		digits++;
		if (ip > 1000000000ULL)   /* keeps (ip * 1000 + frac) * 1e6 inside 64 bits */
		{
			return 0;
		}
	}
	if (*s == '.')
	{
		s++;
		while (*s >= '0' && *s <= '9')
		{
			if (fdig < 3U)
			{
				frac = frac * 10U + (uint64_t)(*s - '0');
				fdig++;
			}
			s++;
			digits++;
		}
	}
	if (digits == 0U)
	{
		return 0;
	}
	while (fdig < 3U)
	{
		frac *= 10U;
		fdig++;
	}
	if (*s == 'K')
	{
		mult = 1000U;
		s++;
	}
	else if (*s == 'M' && s[1] != 'S')
	{
		mult = 1000000U;
		s++;
	}
	if (!(*s == '\0' || strcmp(s, "HZ") == 0 || strcmp(s, "S") == 0 ||
	      strcmp(s, "MS") == 0 || strcmp(s, "DEG") == 0))
	{
		return 0;
	}
	*milli = (ip * 1000U + frac) * mult;
	return 1;
}

static uint8_t parse_hz(const char *s, uint32_t *hz)
{
	uint64_t m;
	if (!parse_milli(s, &m) || (m + 500U) / 1000U > FG_MAX_FREQ)
	{
		return 0;
	}
	*hz = (uint32_t)((m + 500U) / 1000U);
	return 1;
}

static int8_t parse_onoff(const char *s)
{
	if (s == NULL)
	{
		return -1;
	}
	if (strcmp(s, "ON") == 0 || strcmp(s, "1") == 0)
	{
		return 1;
	}
	if (strcmp(s, "OFF") == 0 || strcmp(s, "0") == 0)
	{
		return 0;
	}
	return -1;
}

/* --------------------------------------------------------------- commands */

static void print_status(void)
{
	const FG_Settings *st = FG_GetSettings();
	unsigned ph = (unsigned)st->phase * 1125U;
	uint32_t step = 1U;

	for (uint8_t i = 0; i < st->digit; i++)
	{
		step *= 10U;
	}
	out("FREQ %lu Hz\r\n", (unsigned long)st->freq);
	if (st->sweep_on && st->out_on)
	{
		out("NOW %lu Hz\r\n", (unsigned long)FG_OutputFreq());
	}
	out("STEP %lu Hz\r\n", (unsigned long)step);
	out("PHASE %u.%02u deg\r\n", ph / 100U, ph % 100U);
	out("OUT %s\r\n", st->out_on ? "ON" : "OFF");
	out("SWEEP %s %lu %lu %u.%u s\r\n", st->sweep_on ? "ON" : "OFF",
	    (unsigned long)st->sw_start, (unsigned long)st->sw_stop,
	    (unsigned)(st->sw_time_ds / 10U), (unsigned)(st->sw_time_ds % 10U));
	out("BURST %s %u %u ms\r\n", st->burst_on ? "ON" : "OFF",
	    (unsigned)st->bu_on_ms, (unsigned)st->bu_off_ms);
}

static void print_help(void)
{
	out("F <freq>            frequency, e.g. F 1000, F 12.5k, F 7.1M\r\n"
	    "STEP <freq>         encoder step 1 .. 10M (power of 10)\r\n"
	    "PH <deg>            phase (11.25 deg steps)\r\n"
	    "OUT ON|OFF\r\n"
	    "SWEEP <start> <stop> <s> | SWEEP ON|OFF\r\n"
	    "BURST <on_ms> <off_ms>   | BURST ON|OFF\r\n"
	    "SAVE                write settings to flash\r\n"
	    "? | STATUS          show settings\r\n"
	    "*IDN?\r\n");
}

/* Returns NULL on success, or the error text */
static const char *execute(char *line)
{
	FG_Settings *st = FG_GetSettings();
	char *p = line;
	char *cmd = next_token(&p);
	char *a1 = next_token(&p);
	char *a2 = next_token(&p);
	char *a3 = next_token(&p);
	uint32_t f1, f2;
	uint64_t m;
	int8_t onoff;

	if (cmd == NULL)
	{
		return "";                       /* empty line: no reply */
	}
	if (next_token(&p) != NULL)
	{
		return "too many arguments";
	}

	if (strcmp(cmd, "?") == 0 || strcmp(cmd, "STATUS") == 0)
	{
		print_status();
		return NULL;
	}
	if (strcmp(cmd, "*IDN?") == 0)
	{
		out("DIY,AD9850 DDS Function Generator,%08lX,1.0\r\n", (unsigned long)(*(uint32_t *)UID_BASE));
		return NULL;
	}
	if (strcmp(cmd, "HELP") == 0)
	{
		print_help();
		return NULL;
	}
	if (strcmp(cmd, "F") == 0 || strcmp(cmd, "FREQ") == 0)
	{
		if (a2 != NULL || !parse_hz(a1, &f1))
		{
			return "frequency 0 .. 40M";
		}
		st->freq = f1;
		st->sweep_on = 0;
	}
	else if (strcmp(cmd, "STEP") == 0)
	{
		uint8_t d = 0;
		if (a2 != NULL || !parse_hz(a1, &f1))
		{
			return "step 1 .. 10M";
		}
		f2 = 1U;
		while (f2 < f1 && d < FG_MAX_DIGIT)
		{
			f2 *= 10U;
			d++;
		}
		if (f2 != f1)
		{
			return "step must be 1, 10, 100 .. 10M";
		}
		st->digit = d;
	}
	else if (strcmp(cmd, "PH") == 0 || strcmp(cmd, "PHASE") == 0)
	{
		if (a2 != NULL || !parse_milli(a1, &m) || m >= 360000U)
		{
			return "phase 0 .. 359.99";
		}
		/* steps of 11.25 deg = 11250 milli-degrees, rounded */
		st->phase = (uint8_t)(((m + 5625U) / 11250U) % AD_PHASE_STEPS);
	}
	else if (strcmp(cmd, "OUT") == 0)
	{
		onoff = parse_onoff(a1);
		if (a2 != NULL || onoff < 0)
		{
			return "OUT ON|OFF";
		}
		st->out_on = (uint8_t)onoff;
	}
	else if (strcmp(cmd, "SWEEP") == 0)
	{
		onoff = parse_onoff(a1);
		if (onoff >= 0 && a2 == NULL)
		{
			st->sweep_on = (uint8_t)onoff;
		}
		else
		{
			if (a3 == NULL || !parse_hz(a1, &f1) || !parse_hz(a2, &f2) || !parse_milli(a3, &m))
			{
				return "SWEEP <start> <stop> <seconds> | SWEEP ON|OFF";
			}
			m = (m + 50U) / 100U;        /* tenths of a second */
			if (m < FG_SWT_MIN || m > FG_SWT_MAX)
			{
				return "sweep time 0.1 .. 60 s";
			}
			st->sw_start = f1;
			st->sw_stop = f2;
			st->sw_time_ds = (uint16_t)m;
			st->sweep_on = 1;
		}
	}
	else if (strcmp(cmd, "BURST") == 0)
	{
		onoff = parse_onoff(a1);
		if (onoff >= 0 && a2 == NULL)
		{
			st->burst_on = (uint8_t)onoff;
		}
		else
		{
			uint64_t on_m, off_m;
			if (a2 == NULL || a3 != NULL || !parse_milli(a1, &on_m) || !parse_milli(a2, &off_m))
			{
				return "BURST <on_ms> <off_ms> | BURST ON|OFF";
			}
			on_m = (on_m + 500U) / 1000U;
			off_m = (off_m + 500U) / 1000U;
			if (on_m < FG_BURST_MIN_MS || on_m > FG_BURST_MAX_MS ||
			    off_m < FG_BURST_MIN_MS || off_m > FG_BURST_MAX_MS)
			{
				return "burst times 1 .. 60000 ms";
			}
			st->bu_on_ms = (uint16_t)on_m;
			st->bu_off_ms = (uint16_t)off_m;
			st->burst_on = 1;
		}
	}
	else if (strcmp(cmd, "SAVE") == 0)
	{
		if (a1 != NULL)
		{
			return "SAVE takes no arguments";
		}
		return FG_SaveNow() ? NULL : "flash write failed";
	}
	else
	{
		return "unknown command (HELP)";
	}

	FG_SettingsChanged();
	return NULL;
}

/* ----------------------------------------------------------------- polling */

void UC_Poll(void)
{
	int16_t c;

	while ((c = CDC_ReadByte()) >= 0)
	{
		if (c == '\r' || c == '\n')
		{
			if (s_len == 0U && !s_overflow)
			{
				continue;                /* CR LF pairs, blank lines */
			}
			s_line[s_len] = '\0';
			if (s_overflow)
			{
				out("ERR line too long\r\n");
			}
			else
			{
				const char *err = execute(s_line);
				if (err == NULL)
				{
					out("OK\r\n");
				}
				else if (err[0] != '\0')
				{
					out("ERR %s\r\n", err);
				}
			}
			s_len = 0;
			s_overflow = 0;
			flush();
		}
		else if (c == '\b' || c == 0x7F)
		{
			if (s_len > 0U)
			{
				s_len--;
			}
		}
		else if (c >= ' ' && c < 0x7F)
		{
			if (s_len < UC_LINE_MAX - 1U)
			{
				s_line[s_len++] = (char)((c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c);
			}
			else
			{
				s_overflow = 1;
			}
		}
	}
}
