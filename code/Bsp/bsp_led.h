#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "main.h"
#include <stdint.h>

void bsp_led_on(uint8_t led);
void bsp_led_off(uint8_t led);
void bsp_led_toggle(uint8_t led);
void bsp_led_write8(uint8_t value);

#endif
