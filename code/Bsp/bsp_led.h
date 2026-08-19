#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "main.h"
#include "cmsis_os.h"
#include <stdint.h>

/* LCD 与 LED 共享 GPIOC 数据总线（PC8-PC15），使用互斥锁串行访问 */
extern osMutexId_t g_lcd_led_bus_mutex;

void bsp_led_on(uint8_t led);
void bsp_led_off(uint8_t led);
void bsp_led_toggle(uint8_t led);
void bsp_led_write8(uint8_t value);
uint8_t bsp_led_get_state(void);

void bsp_bus_lock(void);
void bsp_bus_unlock(void);

#endif
