#ifndef __BSP_LCD_H
#define __BSP_LCD_H

#include "main.h"
#include "terminal_data.h"

void bsp_lcd_init(void);
void bsp_lcd_clear(void);

/* OLED 显示：由上层任务传入数据，BSP 层只负责渲染 */
void bsp_lcd_show_test_info(const terminal_data_t *data);

uint8_t bsp_lcd_uart3_loopback_ok(void);

#endif
