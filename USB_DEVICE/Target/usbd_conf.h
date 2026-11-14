/* USER CODE BEGIN Header */
/**
  * @file    usbd_conf.h
  * @brief   USB device low level configuration (STM32F1, USB FS device, CDC).
  *          Layout follows what STM32CubeMX generates for USB_DEVICE / CDC_FS.
  */
/* USER CODE END Header */
#ifndef __USBD_CONF__H__
#define __USBD_CONF__H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"

#define USBD_MAX_NUM_INTERFACES        1U
#define USBD_MAX_NUM_CONFIGURATION     1U
#define USBD_MAX_STR_DESC_SIZ          512U
#define USBD_DEBUG_LEVEL               0U
#define USBD_SELF_POWERED              1U
#define USBD_MAX_POWER                 0x32U   /* 100 mA */

#define DEVICE_FS                      0

/* Memory management: the CDC class allocates its state once */
#define USBD_malloc         (void *)USBD_static_malloc
#define USBD_free           USBD_static_free
#define USBD_memset         memset
#define USBD_memcpy         memcpy
#define USBD_Delay          HAL_Delay

#if (USBD_DEBUG_LEVEL > 0)
#define USBD_UsrLog(...)    do { printf(__VA_ARGS__); printf("\n"); } while (0)
#else
#define USBD_UsrLog(...)    do { } while (0)
#endif
#if (USBD_DEBUG_LEVEL > 1)
#define USBD_ErrLog(...)    do { printf("ERROR: "); printf(__VA_ARGS__); printf("\n"); } while (0)
#else
#define USBD_ErrLog(...)    do { } while (0)
#endif
#if (USBD_DEBUG_LEVEL > 2)
#define USBD_DbgLog(...)    do { printf("DEBUG : "); printf(__VA_ARGS__); printf("\n"); } while (0)
#else
#define USBD_DbgLog(...)    do { } while (0)
#endif

void *USBD_static_malloc(uint32_t size);
void USBD_static_free(void *p);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CONF__H__ */
