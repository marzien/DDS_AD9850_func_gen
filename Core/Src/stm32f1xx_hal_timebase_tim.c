/* USER CODE BEGIN Header */
/**
  * @file    stm32f1xx_hal_timebase_tim.c
  * @brief   HAL time base on TIM4 (1 ms). SysTick is used by FreeRTOS.
  */
/* USER CODE END Header */
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_tim.h"

TIM_HandleTypeDef htim4;

/**
  * @brief  Configure TIM4 as the HAL time base: 1 ms period, update interrupt.
  *         Called by HAL_Init() and again by HAL_RCC_ClockConfig().
  */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  RCC_ClkInitTypeDef clkconfig;
  uint32_t uwTimclock;
  uint32_t uwPrescalerValue;
  uint32_t pFLatency;
  HAL_StatusTypeDef status;

  __HAL_RCC_TIM4_CLK_ENABLE();

  HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);
  /* TIM4 is on APB1: the timer clock is 2 x PCLK1 when the APB1 prescaler is not 1 */
  if (clkconfig.APB1CLKDivider == RCC_HCLK_DIV1)
  {
    uwTimclock = HAL_RCC_GetPCLK1Freq();
  }
  else
  {
    uwTimclock = 2UL * HAL_RCC_GetPCLK1Freq();
  }
  uwPrescalerValue = (uint32_t)((uwTimclock / 1000000U) - 1U);   /* 1 MHz counter */

  htim4.Instance = TIM4;
  htim4.Init.Period = (1000000U / 1000U) - 1U;                    /* 1 kHz */
  htim4.Init.Prescaler = uwPrescalerValue;
  htim4.Init.ClockDivision = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  status = HAL_TIM_Base_Init(&htim4);
  if (status == HAL_OK)
  {
    status = HAL_TIM_Base_Start_IT(&htim4);
    if (status == HAL_OK)
    {
      HAL_NVIC_EnableIRQ(TIM4_IRQn);
      if (TickPriority < (1UL << __NVIC_PRIO_BITS))
      {
        HAL_NVIC_SetPriority(TIM4_IRQn, TickPriority, 0U);
        uwTickPrio = TickPriority;
      }
      else
      {
        status = HAL_ERROR;
      }
    }
  }
  return status;
}

void HAL_SuspendTick(void)
{
  __HAL_TIM_DISABLE_IT(&htim4, TIM_IT_UPDATE);
}

void HAL_ResumeTick(void)
{
  __HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);
}
