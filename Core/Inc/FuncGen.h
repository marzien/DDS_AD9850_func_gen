#ifndef _FUNCGEN_H_
#define _FUNCGEN_H_
/*
 * AD9850 function generator application: rotary encoder UI on a 20x4 I2C LCD.
 *
 * Main screen
 *   rotate       : change frequency by the digit under the cursor
 *   short press  : move the cursor one digit left (1 Hz .. 10 MHz, wraps)
 *   long press   : open the menu
 *   (while sweeping, short press stops the sweep)
 *
 * Menu  (Output, Phase, Sweep, Sw start, Sw stop, Sw time, Exit)
 *   rotate       : choose item            | while editing: change value
 *   short press  : edit item / confirm    | freq items: next digit
 *   long press   : leave menu             | while editing: confirm
 */
#include "stm32f1xx_hal.h"

void FG_Init(I2C_HandleTypeDef *hi2c);
void FG_Task(void); /* call continuously from the main loop */

#endif
