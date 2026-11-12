#include "Rotary_Sw.h"
#include "main.h"

/* index = (previous AB << 2) | current AB ; invalid (double) transitions give 0 */
static const int8_t k_qdec[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

static volatile uint8_t s_ready = 0;
static volatile int16_t s_delta = 0;
static volatile uint8_t s_event = RS_BTN_NONE;

static uint8_t s_ab;
static uint8_t s_rest_ab;
static int8_t  s_acc;

static uint8_t  s_btn_stable;
static uint8_t  s_btn_cnt;
static uint16_t s_btn_hold;
static uint8_t  s_btn_long_sent;

static inline uint8_t read_ab(void)
{
	uint8_t a = (BTN_A_GPIO_Port->IDR & BTN_A_Pin) ? 1U : 0U;
	uint8_t b = (BTN_B_GPIO_Port->IDR & BTN_B_Pin) ? 1U : 0U;
	return (uint8_t)((a << 1) | b);
}

static inline uint8_t read_btn_pressed(void)
{
	uint8_t lvl = (BTN_PUSH_GPIO_Port->IDR & BTN_PUSH_Pin) ? 1U : 0U;
	return (lvl == RS_BTN_ACTIVE_LEVEL) ? 1U : 0U;
}

static inline uint8_t is_rest_state(uint8_t ab)
{
#if RS_STEPS_PER_DETENT >= 4
	return ab == s_rest_ab;
#else
	return (ab == 0U) || (ab == 3U);
#endif
}

void RS_Init(void)
{
	s_ab = read_ab();
	s_rest_ab = s_ab; /* encoder sits in a detent at power-up */
	s_acc = 0;
	s_delta = 0;
	s_btn_stable = read_btn_pressed();
	s_btn_cnt = 0;
	s_btn_hold = 0;
	s_btn_long_sent = 1; /* ignore a button already held at boot */
	s_event = RS_BTN_NONE;
	s_ready = 1;
}

void RS_Poll_1ms(void)
{
	uint8_t ab, pressed;

	if (!s_ready)
	{
		return;
	}

	/* ---- Encoder ---- */
	ab = read_ab();
	if (ab != s_ab)
	{
		s_acc += k_qdec[(s_ab << 2) | ab];
		s_ab = ab;
#if RS_STEPS_PER_DETENT <= 1
		s_delta += s_acc;
		s_acc = 0;
#else
		/* Count one click when we land back in a detent; contact bounce cancels itself. */
		if (is_rest_state(ab))
		{
			if (s_acc >= (RS_STEPS_PER_DETENT / 2))
			{
				s_delta += RS_REVERSE ? -1 : 1;
			}
			else if (s_acc <= -(RS_STEPS_PER_DETENT / 2))
			{
				s_delta += RS_REVERSE ? 1 : -1;
			}
			s_acc = 0;
		}
#endif
	}

	/* ---- Push switch ---- */
	pressed = read_btn_pressed();
	if (pressed != s_btn_stable)
	{
		if (++s_btn_cnt >= RS_DEBOUNCE_MS)
		{
			s_btn_cnt = 0;
			s_btn_stable = pressed;
			if (pressed)
			{
				s_btn_hold = 0;
				s_btn_long_sent = 0;
			}
			else if (!s_btn_long_sent)
			{
				s_event = RS_BTN_SHORT;
			}
		}
	}
	else
	{
		s_btn_cnt = 0;
	}

	if (s_btn_stable && !s_btn_long_sent)
	{
		if (++s_btn_hold >= RS_LONG_PRESS_MS)
		{
			s_btn_long_sent = 1;
			s_event = RS_BTN_LONG;
		}
	}
}

int16_t RS_GetDelta(void)
{
	int16_t d;
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	d = s_delta;
	s_delta = 0;
	__set_PRIMASK(primask);
	return d;
}

RS_BtnEvent RS_GetButton(void)
{
	RS_BtnEvent e;
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	e = (RS_BtnEvent)s_event;
	s_event = RS_BTN_NONE;
	__set_PRIMASK(primask);
	return e;
}
