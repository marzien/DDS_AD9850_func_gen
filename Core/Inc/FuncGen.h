#ifndef _FUNCGEN_H_
#define _FUNCGEN_H_
/*
 * AD9850 function generator: rotary encoder UI on a 20x4 I2C LCD (FreeRTOS UI task).
 *
 * Main screen
 *   rotate       : change frequency by the digit under the cursor
 *   press + turn : change step (CW = bigger, CCW = smaller, 1 Hz .. 10 MHz)
 *   short press  : move the cursor one digit left (1 Hz .. 10 MHz, wraps)
 *   long press   : open the menu
 *   (while sweeping, short press stops the sweep)
 *
 * Menu  (Output, Phase, Burst, On time, Off time, Sweep, Sw start, Sw stop,
 *        Sw time, Exit)
 *   rotate       : choose item            | while editing: change value
 *   short press  : edit item / confirm    | freq items: next digit
 *   press + turn :                        | freq items: change step
 *   long press   : leave menu             | while editing: confirm
 */
#include "stm32f1xx_hal.h"
#include "AD9850.h"

#define FG_MAX_FREQ        AD_MAX_FREQ
#define FG_MAX_DIGIT       7U     /* 10^7 = 10 MHz step */
#define FG_SWT_MIN         1U     /* sweep time, units of 0.1 s */
#define FG_SWT_MAX         600U
#define FG_BURST_MIN_MS    1U     /* burst on/off time limits */
#define FG_BURST_MAX_MS    60000U

typedef struct
{
	uint32_t freq;       /* Hz */
	uint32_t sw_start;   /* Hz */
	uint32_t sw_stop;    /* Hz */
	uint16_t sw_time_ds; /* sweep period, 0.1 s units */
	uint16_t bu_on_ms;   /* burst: output on time */
	uint16_t bu_off_ms;  /* burst: output off time */
	uint8_t  phase;      /* 0..31, 11.25 deg steps */
	uint8_t  digit;      /* main screen edit digit, step = 10^digit */
	uint8_t  out_on;
	uint8_t  sweep_on;
	uint8_t  burst_on;
} FG_Settings;

void FG_LoadSettings(void);   /* before the scheduler: load settings, start the output */
void FG_UiTask(void *arg);    /* UI task: menus, LCD, settings autosave */

/* For remote control: with App_Lock() held, change fields of FG_GetSettings(),
 * then call FG_SettingsChanged() to apply them, refresh the LCD and autosave. */
FG_Settings *FG_GetSettings(void);
void FG_SettingsChanged(void);
uint8_t FG_SaveNow(void);        /* 1 = settings are in flash */
uint32_t FG_OutputFreq(void);    /* frequency on the output now (follows the sweep) */

#endif
