#include "Flash_Store.h"
#include <stddef.h>
#include <string.h>

#define FS_MAGIC      0x5346U /* "FS" */
#define FS_SLOTS      (FS_PAGE_SIZE / FS_SLOT_SIZE)
#define FS_SLOT_WORDS (FS_SLOT_SIZE / 4U)

typedef struct
{
	uint16_t magic;
	uint8_t  version;
	uint8_t  len;
	uint8_t  payload[FS_MAX_PAYLOAD];
	uint32_t crc;
} FS_Slot;

typedef char fs_slot_size_check[(sizeof(FS_Slot) == FS_SLOT_SIZE) ? 1 : -1];

static uint32_t crc32(const uint8_t *p, uint32_t n)
{
	uint32_t c = 0xFFFFFFFFUL;
	while (n--)
	{
		c ^= *p++;
		for (uint8_t k = 0; k < 8U; k++)
		{
			c = (c >> 1) ^ (0xEDB88320UL & (0UL - (c & 1UL)));
		}
	}
	return ~c;
}

static const FS_Slot *slot_ptr(uint32_t i)
{
	return (const FS_Slot *)(FS_PAGE_ADDR + i * FS_SLOT_SIZE);
}

static uint8_t slot_is_erased(uint32_t i)
{
	const uint32_t *w = (const uint32_t *)slot_ptr(i);
	for (uint32_t k = 0; k < FS_SLOT_WORDS; k++)
	{
		if (w[k] != 0xFFFFFFFFUL)
		{
			return 0;
		}
	}
	return 1;
}

static uint8_t slot_is_valid(const FS_Slot *s)
{
	return (s->magic == FS_MAGIC) && (s->len <= FS_MAX_PAYLOAD) &&
	       (s->crc == crc32((const uint8_t *)s, offsetof(FS_Slot, crc)));
}

/* Newest valid slot, or -1 */
static int32_t find_newest(void)
{
	int32_t newest = -1;
	for (uint32_t i = 0; i < FS_SLOTS; i++)
	{
		if (slot_is_valid(slot_ptr(i)))
		{
			newest = (int32_t)i;
		}
	}
	return newest;
}

/* First slot after the last used (non-erased) one, or FS_SLOTS if full */
static uint32_t find_free(void)
{
	uint32_t i = FS_SLOTS;
	while (i > 0U && slot_is_erased(i - 1U))
	{
		i--;
	}
	return i;
}

uint8_t FS_Load(void *data, uint8_t len, uint8_t version)
{
	int32_t i = find_newest();
	const FS_Slot *s;

	if (i < 0)
	{
		return 0;
	}
	s = slot_ptr((uint32_t)i);
	if (s->version != version || s->len != len)
	{
		return 0;
	}
	memcpy(data, s->payload, len);
	return 1;
}

HAL_StatusTypeDef FS_Save(const void *data, uint8_t len, uint8_t version)
{
	FS_Slot rec;
	const uint32_t *w = (const uint32_t *)&rec;
	uint32_t i, addr;
	HAL_StatusTypeDef st = HAL_OK;

	if (len > FS_MAX_PAYLOAD)
	{
		return HAL_ERROR;
	}

	memset(&rec, 0xFF, sizeof(rec));
	rec.magic = FS_MAGIC;
	rec.version = version;
	rec.len = len;
	memcpy(rec.payload, data, len);
	rec.crc = crc32((const uint8_t *)&rec, offsetof(FS_Slot, crc));

	HAL_FLASH_Unlock();

	i = find_free();
	if (i >= FS_SLOTS)
	{
		FLASH_EraseInitTypeDef er;
		uint32_t page_err = 0;
		er.TypeErase = FLASH_TYPEERASE_PAGES;
		er.Banks = FLASH_BANK_1;
		er.PageAddress = FS_PAGE_ADDR;
		er.NbPages = 1;
		st = HAL_FLASHEx_Erase(&er, &page_err);
		i = 0;
	}

	addr = FS_PAGE_ADDR + i * FS_SLOT_SIZE;
	/* Header word (magic) is written last, so an interrupted write never
	 * looks like a valid record. */
	for (uint32_t k = 1; k < FS_SLOT_WORDS && st == HAL_OK; k++)
	{
		st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + k * 4U, w[k]);
	}
	if (st == HAL_OK)
	{
		st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, w[0]);
	}

	HAL_FLASH_Lock();

	if (st == HAL_OK && memcmp(slot_ptr(i), &rec, sizeof(rec)) != 0)
	{
		st = HAL_ERROR;
	}
	return st;
}
