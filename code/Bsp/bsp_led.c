#include "bsp_led.h"

static uint8_t led_state = 0x00;

void led_show(uint8_t led, uint8_t mode)
{
    if (led < 1 || led > 8)
    {
        return;
    }

    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);

    if (mode)
    {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8 << (led - 1), GPIO_PIN_RESET);
    }
    else
    {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8 << (led - 1), GPIO_PIN_SET);
    }

    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);
}

void bsp_led_write8(uint8_t value)
{
    led_state = value;

    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);

    for (uint8_t i = 0; i < 8; i++)
    {
        if (value & (1 << i))
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8 << i, GPIO_PIN_RESET);
        }
        else
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8 << i, GPIO_PIN_SET);
        }
    }

    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);
}

void bsp_led_on(uint8_t led)
{
    if (led < 1 || led > 8)
    {
        return;
    }

    led_state |= (1 << (led - 1));
    bsp_led_write8(led_state);
}

void bsp_led_off(uint8_t led)
{
    if (led < 1 || led > 8)
    {
        return;
    }

    led_state &= ~(1 << (led - 1));
    bsp_led_write8(led_state);
}

void bsp_led_toggle(uint8_t led)
{
    if (led < 1 || led > 8)
    {
        return;
    }

    led_state ^= (1 << (led - 1));
    bsp_led_write8(led_state);
}
