#ifndef _FLASH_STORE_H_
#define _FLASH_STORE_H_
/*
 * Small settings store in the last 1 KB flash page (wear levelled).
 *
 * The page is split into 32-byte slots. Each save appends a new slot
 * (header + payload + CRC32); the newest valid slot is the current data.
 * The page is erased only when all slots are used, so 32 saves cost one
 * erase cycle (flash is rated for 10,000 erase cycles per page).
 *
 * The page sits at the end of the first 64 KB, so it works on both the
 * STM32F103C8 (Blue Pill, 64 KB) and the STM32F103RB (128 KB). The linker
 * script keeps program code out of it (FLASH LENGTH = 63K).
 */
#include <stdint.h>
#include "stm32f1xx_hal.h"

#define FS_PAGE_ADDR    0x0800FC00UL /* last page of 64 KB */
#define FS_PAGE_SIZE    1024U
#define FS_SLOT_SIZE    32U
#define FS_MAX_PAYLOAD  24U          /* slot = 4 header + 24 payload + 4 CRC */

/* Copy the newest stored record into 'data'. Returns 1 if a valid record of
 * exactly 'len' bytes with matching 'version' was found, else 0 (data untouched). */
uint8_t FS_Load(void *data, uint8_t len, uint8_t version);

/* Store 'data' as a new record. Returns HAL_OK on success. */
HAL_StatusTypeDef FS_Save(const void *data, uint8_t len, uint8_t version);

#endif
