#ifndef _I2C_LCD_H_
#define _I2C_LCD_H_
/*
 * HD44780 character LCD driven through a PCF8574 I2C backpack.
 * PCF8574 bit map: P0=RS, P1=RW, P2=EN, P3=Backlight, P4..P7=D4..D7
 */
#include <stdint.h>
#include "stm32f1xx_hal.h"

/* 8-bit (already shifted) I2C address. PCF8574 = 0x4E (0x27<<1), PCF8574A = 0x7E (0x3F<<1).
 * lcd_init() falls back to the other one automatically if this one does not answer. */
#define SLAVE_ADDRESS_LCD 0x4E

#define LCD_COLS 20
#define LCD_ROWS 4

/* Returns HAL_OK when the backpack answered on the I2C bus. */
HAL_StatusTypeDef lcd_init(I2C_HandleTypeDef *hi2c, uint8_t addr);
void lcd_send_cmd(uint8_t cmd);
void lcd_send_data(uint8_t data);
void lcd_send_string(const char *str);
void lcd_clear(void);
void lcd_put_cur(uint8_t row, uint8_t col);
/* Writes a whole row, padded with spaces to LCD_COLS. */
void lcd_print_line(uint8_t row, const char *str);
void lcd_cursor(uint8_t show, uint8_t blink);
void lcd_backlight(uint8_t on);

#endif
