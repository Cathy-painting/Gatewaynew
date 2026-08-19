#include "bsp_rs485.h"
#include "gpio.h"
#include "main.h"
#include "usart.h"
#include "bsp_uart.h"

#define RS485_DE_GPIO_Port GPIOB
#define RS485_DE_Pin GPIO_PIN_13

extern UART_HandleTypeDef huart3;

/* 使用 DWT 周期计数器实现精确微秒延时，替代不精确的软件循环 */
static void bsp_rs485_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t wait  = us * (SystemCoreClock / 1000000UL);
    while ((DWT->CYCCNT - start) < wait) { }
}

void bsp_rs485_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 启用 DWT 周期计数器（用于精确延时） */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = RS485_DE_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RS485_DE_GPIO_Port, &GPIO_InitStruct);
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
    bsp_rs485_delay_us(RS485_TURNAROUND_US);

    /* HAL_UART_Transmit 已经是阻塞等待 TC 的，不需要再额外死等 */
    if (HAL_UART_Transmit(&huart3, (uint8_t *)data, len, RS485_TX_TIMEOUT_MS) != HAL_OK) {
        /* 发送失败也切回接收模式，避免卡死 */
    }

    /* 等待最后一个字节的停止位发送完毕再切回收模式 */
    bsp_rs485_delay_us(RS485_TURNAROUND_US);
    bsp_rs485_set_rx_mode();

    bsp_uart3_rx_restart();
}
