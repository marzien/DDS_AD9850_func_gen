#include "Dds.h"
#include "main.h"
#include "AD9850.h"
#include "FreeRTOS.h"
#include "task.h"

static FG_Settings       s_new;          /* handed over by DDS_Configure() */
static volatile uint8_t  s_new_pending = 0;
static volatile uint32_t s_cur_freq = 0;

void DDS_Init(void)
{
	AD_CFG ad_cfg;

	ad_cfg.W_CLK_PORT = W_CLK_GPIO_Port;
	ad_cfg.W_CLK_PIN  = W_CLK_Pin;
	ad_cfg.FU_FD_PORT = FU_UD_GPIO_Port;
	ad_cfg.FU_FD_PIN  = FU_UD_Pin;
	ad_cfg.DATA_PORT  = Data_GPIO_Port;
	ad_cfg.DATA_PIN   = Data_Pin;
	AD_Init(&ad_cfg);
}

void DDS_Configure(const FG_Settings *set)
{
	taskENTER_CRITICAL();
	s_new = *set;
	s_new_pending = 1;
	taskEXIT_CRITICAL();
}

uint32_t DDS_CurrentFreq(void)
{
	return s_cur_freq;
}

void DDS_Task(void *arg)
{
	FG_Settings cfg = {0};
	TickType_t  last = xTaskGetTickCount();
	uint32_t    gate_t = 0;       /* ms inside the burst cycle */
	uint32_t    sweep_t = 0;      /* ms inside the sweep period */
	uint32_t    freq = 0;
	uint8_t     gate_open = 1;
	uint8_t     have_cfg = 0;

	(void)arg;
	for (;;)
	{
		uint8_t write = 0;

		(void)xTaskDelayUntil(&last, pdMS_TO_TICKS(1));

		if (s_new_pending)
		{
			taskENTER_CRITICAL();
			cfg = s_new;
			s_new_pending = 0;
			taskEXIT_CRITICAL();
			have_cfg = 1;

			/* New settings restart the sweep and the burst cycle (on time first) */
			gate_t = 0;
			gate_open = 1;
			sweep_t = 0;
			freq = cfg.sweep_on ? cfg.sw_start : cfg.freq;
			s_cur_freq = freq;
			if (cfg.out_on)
			{
				AD_Set_Freq(freq, cfg.phase);
			}
			else
			{
				AD_Power_Down();
			}
			continue;
		}
		if (!have_cfg || !cfg.out_on)
		{
			continue;
		}

		if (cfg.sweep_on)
		{
			uint32_t period = (uint32_t)cfg.sw_time_ds * 100U;   /* ms, multiple of 100 */

			if (++sweep_t >= period)
			{
				sweep_t = 0;
			}
			if ((sweep_t % DDS_SWEEP_UPDATE_MS) == 0U)
			{
				int64_t span = (int64_t)cfg.sw_stop - (int64_t)cfg.sw_start;   /* negative = down */
				freq = (uint32_t)((int64_t)cfg.sw_start + (span * (int64_t)sweep_t) / (int64_t)period);
				s_cur_freq = freq;
				write = 1;
			}
		}

		if (cfg.burst_on)
		{
			uint8_t open;

			if (++gate_t >= ((uint32_t)cfg.bu_on_ms + cfg.bu_off_ms))
			{
				gate_t = 0;
			}
			open = (uint8_t)(gate_t < cfg.bu_on_ms);
			if (open != gate_open)
			{
				gate_open = open;
				if (open)
				{
					write = 1;            /* (re)start the output at the current frequency */
				}
				else
				{
					AD_Power_Down();
					write = 0;
				}
			}
		}

		/* While the gate is closed a new sweep frequency is only remembered */
		if (write && gate_open)
		{
			AD_Set_Freq(freq, cfg.phase);
		}
	}
}
