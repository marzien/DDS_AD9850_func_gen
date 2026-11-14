/* USER CODE BEGIN Header */
/**
  * @file    usbd_cdc_if.h
  * @brief   CDC virtual COM port interface.
  */
/* USER CODE END Header */
#ifndef __USBD_CDC_IF_H__
#define __USBD_CDC_IF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_cdc.h"

#define APP_RX_DATA_SIZE  64
#define APP_TX_DATA_SIZE  64

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);

/* USER CODE BEGIN EXPORTED_FUNCTIONS */
/* Received bytes are queued in a ring buffer (filled from the USB interrupt). */
int16_t CDC_ReadByte(void);          /* next byte, or -1 if none */
uint8_t CDC_IsConnected(void);       /* 1 = enumerated by the host */
uint8_t CDC_TxBusy(void);            /* 1 = previous transmit still running */
void CDC_RxCallback(void);           /* weak; called from the USB interrupt on received data */
/* USER CODE END EXPORTED_FUNCTIONS */

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CDC_IF_H__ */
