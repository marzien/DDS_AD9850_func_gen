#ifndef _APP_H_
#define _APP_H_
/*
 * RTOS application: creates the tasks (all statically allocated) and starts
 * the scheduler.
 *
 *   Task   Prio  Stack   Job
 *   DDS     4    640 B   owns the AD9850: settings, sweep steps, burst gate (1 ms)
 *   INPUT   3    512 B   polls the encoder and button every 1 ms
 *   UI      2   1536 B   menus, LCD, settings autosave
 *   USB     1   1536 B   command parser on the USB virtual COM port
 *   IDLE    0    384 B   FreeRTOS idle task
 *
 * Interrupts (USB, HAL time base on TIM4) run on the main stack (1.5 KB).
 */
#include <stdint.h>

#define APP_EV_INPUT   0x01UL   /* encoder / button activity */
#define APP_EV_REMOTE  0x02UL   /* settings changed by a USB command */

void App_Start(void);              /* never returns */

void App_Lock(void);               /* settings mutex (UI and USB tasks) */
void App_Unlock(void);
void App_NotifyUi(uint32_t events);
void App_NotifyUsbFromISR(void);

/* Print flash / RAM use and the minimum free stack of every task */
void App_MemReport(void (*print)(const char *fmt, ...));

#endif
