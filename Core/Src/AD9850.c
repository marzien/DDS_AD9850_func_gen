#include "AD9850.h"

static AD_CFG g_AD_CFG;

static inline void pin_high(GPIO_TypeDef *port, uint32_t pin) { port->BSRR = pin; }
static inline void pin_low(GPIO_TypeDef *port, uint32_t pin)  { port->BRR = pin; }

/* At 72 MHz two back-to-back BSRR/BRR writes give a ~30 ns pulse, too short for
 * long wires or the level shifter. Each ad_delay() is ~100-400 ns depending on
 * optimisation (the AD9850 needs >= 3.5 ns for W_CLK/FQ_UD high/low and for data
 * setup/hold). A full 40-bit word still takes < 50 us. */
static inline void ad_delay(void)
{
	for (volatile uint8_t i = 0; i < 2U; i++)
	{
		__NOP();
	}
}

static inline void pulse(GPIO_TypeDef *port, uint32_t pin)
{
	pin_high(port, pin);
	ad_delay();
	pin_low(port, pin);
	ad_delay();
}

static void shift_bits(uint32_t value, uint8_t count)
{
	for (uint8_t i = 0; i < count; i++)
	{
		if ((value >> i) & 0x01U)
		{
			pin_high(g_AD_CFG.DATA_PORT, g_AD_CFG.DATA_PIN);
		}
		else
		{
			pin_low(g_AD_CFG.DATA_PORT, g_AD_CFG.DATA_PIN);
		}
		ad_delay(); /* data setup before W_CLK rises */
		pulse(g_AD_CFG.W_CLK_PORT, g_AD_CFG.W_CLK_PIN);
	}
}

void AD_Init(const AD_CFG *i_ad_cfg)
{
	g_AD_CFG = *i_ad_cfg;

	pin_low(g_AD_CFG.W_CLK_PORT, g_AD_CFG.W_CLK_PIN);
	pin_low(g_AD_CFG.FU_FD_PORT, g_AD_CFG.FU_FD_PIN);
	pin_low(g_AD_CFG.DATA_PORT, g_AD_CFG.DATA_PIN);

	/* Serial mode entry (datasheet fig. 10): with D0=D1=1, D2=0 hardwired on the module,
	 * pulse W_CLK then FQ_UD. */
	pulse(g_AD_CFG.W_CLK_PORT, g_AD_CFG.W_CLK_PIN);
	pulse(g_AD_CFG.FU_FD_PORT, g_AD_CFG.FU_FD_PIN);

	/* Load a defined state: 0 Hz, powered up */
	AD_Set_Freq(0, 0);
}

void AD_Send_Cmd(const AD_MSG *i_msg)
{
	shift_bits(i_msg->freq, 32);      /* W0..W31, LSB first */
	shift_bits(i_msg->ctrl_phase, 8); /* W32..W39 */
	pulse(g_AD_CFG.FU_FD_PORT, g_AD_CFG.FU_FD_PIN); /* latch */
}

void AD_Power_Down(void)
{
	AD_MSG msg = {0, 0x04};
	AD_Send_Cmd(&msg);
}

uint32_t AD_Freq_To_Word(uint32_t i_freq)
{
	/* word = f * 2^32 / fclk, rounded */
	return (uint32_t)((((uint64_t)i_freq << 32) + (OSC_FREQ / 2U)) / OSC_FREQ);
}

void AD_Set_Freq(uint32_t i_freq, uint8_t i_phase)
{
	AD_MSG msg;

	if (i_freq > AD_MAX_FREQ)
	{
		i_freq = AD_MAX_FREQ;
	}
	msg.freq = AD_Freq_To_Word(i_freq);
	msg.ctrl_phase = (uint8_t)((i_phase & 0x1FU) << 3);
	AD_Send_Cmd(&msg);
}
