#include "I2C_LCD.h"
#include <string.h>

#define LCD_RS 0x01U
#define LCD_EN 0x04U
#define LCD_BL 0x08U

#define LCD_I2C_TIMEOUT 10U

static I2C_HandleTypeDef *g_hi2c = NULL;
static uint8_t g_addr = SLAVE_ADDRESS_LCD;
static uint8_t g_bl = LCD_BL;
static uint8_t g_disp_ctrl = 0x0CU; /* display on, cursor off, blink off */

static void lcd_write_raw(uint8_t *buf, uint16_t len)
{
	if (g_hi2c != NULL)
	{
		HAL_I2C_Master_Transmit(g_hi2c, g_addr, buf, len, LCD_I2C_TIMEOUT);
	}
}

/* Fills 4 bytes: high nibble with EN pulse, then low nibble with EN pulse. */
static void lcd_pack_byte(uint8_t *out, uint8_t val, uint8_t rs)
{
	uint8_t flags = (uint8_t)(g_bl | (rs ? LCD_RS : 0U));
	uint8_t hi = (uint8_t)(val & 0xF0U);
	uint8_t lo = (uint8_t)((val << 4) & 0xF0U);
	out[0] = (uint8_t)(hi | flags | LCD_EN);
	out[1] = (uint8_t)(hi | flags);
	out[2] = (uint8_t)(lo | flags | LCD_EN);
	out[3] = (uint8_t)(lo | flags);
}

/* Only used during the power-on reset sequence (controller still in 8-bit mode). */
static void lcd_write_nibble(uint8_t nibble)
{
	uint8_t buf[2];
	uint8_t hi = (uint8_t)((nibble << 4) & 0xF0U);
	buf[0] = (uint8_t)(hi | g_bl | LCD_EN);
	buf[1] = (uint8_t)(hi | g_bl);
	lcd_write_raw(buf, 2);
}

void lcd_send_cmd(uint8_t cmd)
{
	uint8_t buf[4];
	lcd_pack_byte(buf, cmd, 0);
	lcd_write_raw(buf, 4);
	if (cmd <= 0x03U)
	{
		HAL_Delay(2); /* clear / home need 1.52 ms */
	}
}

void lcd_send_data(uint8_t data)
{
	uint8_t buf[4];
	lcd_pack_byte(buf, data, 1);
	lcd_write_raw(buf, 4);
}

void lcd_send_string(const char *str)
{
	/* Send up to 8 characters per I2C transaction to save address overhead. */
	uint8_t buf[32];
	while (*str)
	{
		uint16_t n = 0;
		while (*str && n < sizeof(buf))
		{
			lcd_pack_byte(&buf[n], (uint8_t)*str++, 1);
			n += 4;
		}
		lcd_write_raw(buf, n);
	}
}

HAL_StatusTypeDef lcd_init(I2C_HandleTypeDef *hi2c, uint8_t addr)
{
	HAL_StatusTypeDef st;

	g_hi2c = hi2c;
	g_addr = addr;

	HAL_Delay(50); /* >40 ms after power-up */

	st = HAL_I2C_IsDeviceReady(hi2c, addr, 2, LCD_I2C_TIMEOUT);
	if (st != HAL_OK)
	{
		/* Try the other common backpack address */
		uint8_t alt = (addr == 0x4EU) ? 0x7EU : 0x4EU;
		if (HAL_I2C_IsDeviceReady(hi2c, alt, 2, LCD_I2C_TIMEOUT) == HAL_OK)
		{
			g_addr = alt;
			st = HAL_OK;
		}
	}

	/* HD44780 reset-by-instruction, then switch to 4-bit mode */
	lcd_write_nibble(0x03);
	HAL_Delay(5);
	lcd_write_nibble(0x03);
	HAL_Delay(1);
	lcd_write_nibble(0x03);
	HAL_Delay(1);
	lcd_write_nibble(0x02);
	HAL_Delay(1);

	lcd_send_cmd(0x28); /* 4-bit, 2 lines, 5x8 font */
	lcd_send_cmd(0x08); /* display off */
	lcd_send_cmd(0x01); /* clear */
	lcd_send_cmd(0x06); /* increment, no shift */
	g_disp_ctrl = 0x0CU;
	lcd_send_cmd(g_disp_ctrl); /* display on */

	return st;
}

void lcd_clear(void)
{
	lcd_send_cmd(0x01);
}

void lcd_put_cur(uint8_t row, uint8_t col)
{
	static const uint8_t row_addr[4] = {0x00, 0x40, 0x14, 0x54};
	if (row >= 4U)
	{
		row = 0;
	}
	lcd_send_cmd((uint8_t)(0x80U | (row_addr[row] + col)));
}

void lcd_print_line(uint8_t row, const char *str)
{
	char line[LCD_COLS + 1];
	size_t len = strlen(str);
	if (len > LCD_COLS)
	{
		len = LCD_COLS;
	}
	memcpy(line, str, len);
	memset(&line[len], ' ', LCD_COLS - len);
	line[LCD_COLS] = '\0';

	lcd_put_cur(row, 0);
	lcd_send_string(line);
}

void lcd_cursor(uint8_t show, uint8_t blink)
{
	g_disp_ctrl = (uint8_t)(0x0CU | (show ? 0x02U : 0U) | (blink ? 0x01U : 0U));
	lcd_send_cmd(g_disp_ctrl);
}

void lcd_backlight(uint8_t on)
{
	uint8_t b;
	g_bl = on ? LCD_BL : 0U;
	b = g_bl;
	lcd_write_raw(&b, 1);
}
