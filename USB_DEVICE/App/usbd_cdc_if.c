/* USER CODE BEGIN Header */
/**
  * @file    usbd_cdc_if.c
  * @brief   CDC virtual COM port interface: line coding, receive ring buffer, transmit.
  */
/* USER CODE END Header */
#include "usbd_cdc_if.h"

/* USER CODE BEGIN INCLUDE */
/* USER CODE END INCLUDE */

uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

extern USBD_HandleTypeDef hUsbDeviceFS;

/* USER CODE BEGIN PRIVATE_VARIABLES */
#define CDC_RING_SIZE 256U   /* power of two */
static volatile uint8_t  s_ring[CDC_RING_SIZE];
static volatile uint16_t s_head = 0;   /* written by the USB interrupt */
static volatile uint16_t s_tail = 0;   /* read by the main loop */

/* Terminal programs read the line coding back; keep what the host sets */
static uint8_t s_line_coding[7] = {0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08}; /* 115200 8N1 */
/* USER CODE END PRIVATE_VARIABLES */

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *pbuf, uint32_t *Len);
static int8_t CDC_TransmitCplt_FS(uint8_t *pbuf, uint32_t *Len, uint8_t epnum);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS,
  CDC_TransmitCplt_FS
};

static int8_t CDC_Init_FS(void)
{
  /* USER CODE BEGIN 3 */
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
  s_head = 0;
  s_tail = 0;
  return (USBD_OK);
  /* USER CODE END 3 */
}

static int8_t CDC_DeInit_FS(void)
{
  /* USER CODE BEGIN 4 */
  return (USBD_OK);
  /* USER CODE END 4 */
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
  /* USER CODE BEGIN 5 */
  switch (cmd)
  {
  case CDC_SET_LINE_CODING:
    if (length >= sizeof(s_line_coding))
    {
      memcpy(s_line_coding, pbuf, sizeof(s_line_coding));
    }
    break;
  case CDC_GET_LINE_CODING:
    memcpy(pbuf, s_line_coding, sizeof(s_line_coding));
    break;
  default:
    /* SET_CONTROL_LINE_STATE, SEND_BREAK, ...: nothing to do for a virtual port */
    break;
  }
  return (USBD_OK);
  /* USER CODE END 5 */
}

static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
  /* USER CODE BEGIN 6 */
  for (uint32_t i = 0; i < *Len; i++)
  {
    uint16_t next = (uint16_t)((s_head + 1U) & (CDC_RING_SIZE - 1U));
    if (next == s_tail)
    {
      break;                 /* buffer full: drop the rest */
    }
    s_ring[s_head] = Buf[i];
    s_head = next;
  }
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  CDC_RxCallback();
  return (USBD_OK);
  /* USER CODE END 6 */
}

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len)
{
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 7 */
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
  if (hcdc == NULL)
  {
    return USBD_FAIL;
  }
  if (hcdc->TxState != 0U)
  {
    return USBD_BUSY;
  }
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
  result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);
  /* USER CODE END 7 */
  return result;
}

static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
  int8_t result = USBD_OK;
  /* USER CODE BEGIN 13 */
  (void)Buf;
  (void)Len;
  (void)epnum;
  /* USER CODE END 13 */
  return result;
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */
/* Runs in the USB interrupt after data was queued; override to wake a task */
__weak void CDC_RxCallback(void)
{
}

int16_t CDC_ReadByte(void)
{
  uint8_t b;
  if (s_tail == s_head)
  {
    return -1;
  }
  b = s_ring[s_tail];
  s_tail = (uint16_t)((s_tail + 1U) & (CDC_RING_SIZE - 1U));
  return b;
}

uint8_t CDC_IsConnected(void)
{
  return (uint8_t)(hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED);
}

uint8_t CDC_TxBusy(void)
{
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
  return (uint8_t)((hcdc != NULL) && (hcdc->TxState != 0U));
}
/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */
