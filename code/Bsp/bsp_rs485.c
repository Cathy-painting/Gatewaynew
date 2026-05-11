#include "bsp_rs485.h"
#include "gpio.h"
#include "main.h"
#include "usart.h"

#define RS485_DE_GPIO_Port GPIOB
#define RS485_DE_Pin GPIO_PIN_12

extern UART_HandleTypeDef huart2;

static void bsp_rs485_short_delay(void)
{
    for (volatile uint32_t i = 0; i < 2000; i++) {
    }
}

void bsp_rs485_set_tx_mode(void)
{
    HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_SET);
}

void bsp_rs485_set_rx_mode(void)
{
    HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_RESET);
}

void bsp_rs485_send(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) {
        return;
    }

    bsp_rs485_set_tx_mode();
    bsp_rs485_short_delay();
    HAL_UART_Transmit(&huart2, (uint8_t *)data, len, 500);
    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) == RESET) {
    }
    bsp_rs485_short_delay();
    bsp_rs485_set_rx_mode();
}
