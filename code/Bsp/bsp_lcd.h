#ifndef __BSP_LCD_H
#define __BSP_LCD_H

#include "main.h"

void bsp_lcd_init(void);
void bsp_lcd_clear(void);
void bsp_lcd_show_test_info(void);

void bsp_lcd_uart3_rx_add(uint16_t len);
uint32_t bsp_lcd_uart3_rx_count(void);
uint8_t bsp_lcd_uart3_loopback_ok(void);

#endif
