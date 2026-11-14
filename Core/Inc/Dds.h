#ifndef _DDS_H_
#define _DDS_H_
/*
 * DDS output engine. One task owns the AD9850: it applies new settings, steps
 * the sweep and runs the burst gate on the 1 ms RTOS tick. No other code
 * writes to the AD9850 once the scheduler runs.
 */
#include <stdint.h>
#include "FuncGen.h"

#define DDS_SWEEP_UPDATE_MS 5U   /* frequency update interval while sweeping */

void DDS_Init(void);                          /* before the scheduler starts */
void DDS_Configure(const FG_Settings *set);   /* any task: apply new settings (restarts sweep/burst) */
uint32_t DDS_CurrentFreq(void);               /* frequency being generated now (follows the sweep) */
void DDS_Task(void *arg);

#endif
