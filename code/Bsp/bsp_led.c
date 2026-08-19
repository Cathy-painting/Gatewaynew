#include "bsp_led.h"

static uint8_t led_state = 0x00;

osMutexId_t g_lcd_led_bus_mutex = NULL;

void bsp_bus_lock(void)
{
    if (g_lcd_led_bus_mutex != NULL) {
        if (osMutexAcquire(g_lcd_led_bus_mutex, 100U) != osOK) {
            /* 超时获取锁失败，避免死锁 */
        }
    }
}

void bsp_bus_unlock(void)
{
    if (g_lcd_led_bus_mutex != NULL) {
        (void)osMutexRelease(g_lcd_led_bus_mutex);
    }
}

void bsp_led_write8(uint8_t value)
{
    led_state = value;

    bsp_bus_lock();

    /* 打开 74HC573 锁存使能，PC8-PC15 直通到 LED */
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);

    /* 原子更新 PC8-PC15：bit=1 对应 LED 亮 -> GPIO 输出低电平 */
    GPIOC->ODR = (GPIOC->ODR & 0x00FFU) |
                 (((uint16_t)(~value) << 8) & 0xFF00U);

    /* 关闭锁存，保存 LED 状态 */
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);

    bsp_bus_unlock();
}

void bsp_led_on(uint8_t led)
{
    if (led < 1 || led > 8) return;
    led_state |= (1 << (led - 1));
    bsp_led_write8(led_state);
}

void bsp_led_off(uint8_t led)
{
    if (led < 1 || led > 8) return;
    led_state &= ~(1 << (led - 1));
    bsp_led_write8(led_state);
}

void bsp_led_toggle(uint8_t led)
{
    if (led < 1 || led > 8) return;
    led_state ^= (1 << (led - 1));
    bsp_led_write8(led_state);
}

uint8_t bsp_led_get_state(void)
{
    return led_state;
}
