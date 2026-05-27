#include "bsp_uart.h"
#include "string.h"

uint8_t uart1_rx_byte = 0;
uint8_t uart2_rx_byte = 0;
uint8_t uart3_rx_byte = 0;

uint8_t uart2_rx_buf[256] = {0};
uint16_t uart2_rx_len = 0;
uint8_t uart3_rx_buf[256] = {0};
uint16_t uart3_rx_len = 0;



void bsp_uart1_start_receive(void)
{
    HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
}

void bsp_uart2_start_receive(void)
{
    HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);
}

void bsp_uart3_start_receive(void)
{
    HAL_UART_Receive_IT(&huart3, &uart3_rx_byte, 1);
}

void bsp_uart3_rx_start(void)
{
    uart3_rx_len = 0;
    memset(uart3_rx_buf, 0, sizeof(uart3_rx_buf));
    HAL_UART_Receive_IT(&huart3, &uart3_rx_byte, 1);
}
void bsp_uart2_rx_start(void)
{
    uart2_rx_len = 0;
    memset(uart2_rx_buf, 0, sizeof(uart2_rx_buf));
    HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);
}

void bsp_uart1_send_string(const char *str)
{
    if (str == NULL) {
        return;
    }
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), 100);
}

void bsp_uart2_send_string(const char *str)
{
    if (str == NULL) {
        return;
    }
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), 100);
}

void bsp_uart3_send_string(const char *str)
{
    if (str == NULL) {
        return;
    }
    HAL_UART_Transmit(&huart3, (uint8_t *)str, strlen(str), 100);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        if (uart2_rx_len < sizeof(uart2_rx_buf) - 1)
        {
            uart2_rx_buf[uart2_rx_len] = uart2_rx_byte;
            uart2_rx_len++;
        }
        HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);
    }
    else if (huart->Instance == USART3)
    {
        if (uart3_rx_len < sizeof(uart3_rx_buf) - 1)
        {
            uart3_rx_buf[uart3_rx_len] = uart3_rx_byte;
            uart3_rx_len++;
        }
        HAL_UART_Receive_IT(&huart3, &uart3_rx_byte, 1);
    }
}
