#ifndef _AD9850_H_
#define _AD9850_H_
#include <stdint.h>
#include "stm32f1xx_hal.h"

typedef struct _AD_DATA_PACK
{
	uint32_t freq;       /* 32-bit frequency tuning word */
	uint8_t  ctrl_phase; /* bits 0-1 control (must be 0), bit 2 power-down, bits 3-7 phase */
} AD_MSG;

typedef struct _AD_PIN_CFG
{
	/* Port definition */
	GPIO_TypeDef *W_CLK_PORT;
	GPIO_TypeDef *FU_FD_PORT; /* FQ_UD */
	GPIO_TypeDef *DATA_PORT;  /* D7 on the module */
	/* Pin definition */
	uint32_t W_CLK_PIN;
	uint32_t FU_FD_PIN;
	uint32_t DATA_PIN;
} AD_CFG;

#define OSC_FREQ     125000000UL /* AD9850 reference clock in Hz (module crystal) */
#define AD_MAX_FREQ  40000000UL  /* practical upper limit of the module's output filter */
#define AD_PHASE_STEPS 32U       /* phase resolution: 360/32 = 11.25 deg */

void AD_Init(const AD_CFG *i_ad_cfg); /* stores pins and switches the chip to serial load mode */
void AD_Send_Cmd(const AD_MSG *i_msg);
void AD_Power_Down(void);
/* i_freq in Hz (clamped to AD_MAX_FREQ), i_phase in 11.25 deg steps (0..31) */
void AD_Set_Freq(uint32_t i_freq, uint8_t i_phase);
uint32_t AD_Freq_To_Word(uint32_t i_freq);

#endif
