#ifndef _USBCMD_H_
#define _USBCMD_H_
/*
 * Text command interface on the Blue Pill micro-USB port (USB CDC virtual COM port).
 * Any baud rate works. One command per line (CR and/or LF), case-insensitive.
 * Every command answers "OK" or "ERR <reason>"; '?' prints the current settings.
 *
 *   F <freq>              set frequency, e.g. F 1000, F 12.5k, F 7.1M (stops a sweep)
 *   STEP <freq>           encoder step, a power of ten: 1 .. 10M
 *   PH <degrees>          phase, rounded to 11.25 deg steps
 *   OUT ON|OFF
 *   SWEEP <start> <stop> <seconds>    set and start a sweep (0.1 .. 60 s)
 *   SWEEP ON|OFF
 *   BURST <on_ms> <off_ms>            set and start burst mode (1 .. 60000 ms)
 *   BURST ON|OFF
 *   SAVE                  write settings to flash now
 *   ? | STATUS            print all settings
 *   MEM                   flash / RAM use and minimum free stack of every task
 *   *IDN?                 identification
 *   HELP
 */
#include <stdint.h>

void UC_Task(void *arg);   /* USB command task, woken by received data */

#endif
