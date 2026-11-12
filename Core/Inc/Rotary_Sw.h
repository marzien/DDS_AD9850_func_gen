#ifndef _ROTART_SW_H_
#define _ROTART_SW_H_
/*
 * Rotary encoder (A = BTN_A, B = BTN_B) + push switch (BTN_PUSH).
 * RS_Poll_1ms() must be called every 1 ms (from SysTick_Handler).
 * Turning while the button is held is reported separately (RS_GetHeldDelta) and
 * cancels the short/long press event for that press.
 */
#include <stdint.h>
#include "stm32f1xx_hal.h"

/* Quadrature transitions per mechanical click. KY-040 style = 4.
 * If one click moves the value by 2, set 2; if it needs two clicks, set 4. */
#ifndef RS_STEPS_PER_DETENT
#define RS_STEPS_PER_DETENT 4
#endif

/* Set to 1 if turning clockwise decreases the value. */
#ifndef RS_REVERSE
#define RS_REVERSE 0
#endif

/* Pin level when the push switch is pressed. BTN_PUSH (PA5) has a pull-up
 * (internal, plus optional external 10k to 3V3) and the KY-040 switch connects
 * the pin to GND, so it is active low. Use 1 for a switch wired to 3V3
 * with a pull-down instead. */
#ifndef RS_BTN_ACTIVE_LEVEL
#define RS_BTN_ACTIVE_LEVEL 0
#endif

#define RS_DEBOUNCE_MS   20U
#define RS_LONG_PRESS_MS 700U

typedef enum
{
	RS_BTN_NONE = 0,
	RS_BTN_SHORT,
	RS_BTN_LONG
} RS_BtnEvent;

void RS_Init(void);
void RS_Poll_1ms(void);
int16_t RS_GetDelta(void);      /* clicks since last call, +CW / -CCW */
int16_t RS_GetHeldDelta(void);  /* clicks turned while the button was held down */
RS_BtnEvent RS_GetButton(void); /* last button event, cleared on read */

#endif
