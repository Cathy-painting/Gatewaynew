#include "bsp_uart.h"
#include "main.h"
#include "bsp_lcd.h"
#include <string.h>

#define BSP_UART_RX_BUF_SIZE 256U

uint8_t uart1_rx_byte = 0;
uint8_t uart1_rx_buf[BSP_UART_RX_BUF_SIZE] = {0};
volatile uint16_t uart1_rx_len = 0;

uint8_t uart2_rx_byte = 0;
uint8_t uart2_rx_buf[BSP_UART_RX_BUF_SIZE] = {0};
volatile uint16_t uart2_rx_len = 0;

uint8_t uart3_rx_byte = 0;
uint8_t uart3_rx_buf[BSP_UART_RX_BUF_SIZE] = {0};
volatile uint16_t uart3_rx_len = 0;

static void bsp_uart_send(UART_HandleTypeDef *huart, const uint8_t *data, uint16_t len)
{
    if (huart == NULL || data == NULL || len == 0U) {
        return;
    }

    HAL_UART_Transmit(huart, (uint8_t *)data, len, 1000);
}

static void bsp_uart_start_it(UART_HandleTypeDef *huart, uint8_t *byte)
{
    if (huart == NULL || byte == NULL) {
        return;
    }

    HAL_UART_Receive_IT(huart, byte, 1);
}

static void bsp_uart_restart_it(UART_HandleTypeDef *huart, uint8_t *byte)
{
    if (huart == NULL || byte == NULL) {
        return;
    }

    if (HAL_UART_Receive_IT(huart, byte, 1) != HAL_OK) {
        huart->pRxBuffPtr = byte;
        huart->RxXferSize = 1U;
        huart->RxXferCount = 1U;
        huart->RxState = HAL_UART_STATE_BUSY_RX;
        __HAL_UART_ENABLE_IT(huart, UART_IT_RXNE);
    }
}

static void bsp_uart_reset_buffer(uint8_t *buf, volatile uint16_t *len, uint16_t size)
{
    if (buf == NULL || len == NULL) {
        return;
    }

    __disable_irq();
    *len = 0U;
    memset(buf, 0, size);
    __enable_irq();
}

static uint16_t bsp_uart_copy_buffer(uint8_t *src, volatile uint16_t *src_len, uint8_t *dst, uint16_t dst_size)
{
    uint16_t copy_len = 0U;

    if (src == NULL || src_len == NULL || dst == NULL || dst_size == 0U) {
        return 0U;
    }

    __disable_irq();
    copy_len = *src_len;
    if (copy_len > dst_size) {
        copy_len = dst_size;
    }
    if (copy_len > 0U) {
        memcpy(dst, src, copy_len);
        *src_len = 0U;
    }
    __enable_irq();

    return copy_len;
}

static void bsp_uart_rx_append(uint8_t byte, uint8_t *buf, volatile uint16_t *len, uint16_t size)
{
    uint16_t write_index;

    if (buf == NULL || len == NULL || size == 0U) {
        return;
    }

    write_index = *len;
    if (write_index < (uint16_t)(size - 1U)) {
        buf[write_index] = byte;
        *len = (uint16_t)(write_index + 1U);
    }
}

void bsp_uart1_rx_start(void)
{
    bsp_uart_reset_buffer(uart1_rx_buf, &uart1_rx_len, sizeof(uart1_rx_buf));
    bsp_uart_start_it(&huart1, &uart1_rx_byte);
}

void bsp_uart2_rx_start(void)
{
    bsp_uart_reset_buffer(uart2_rx_buf, &uart2_rx_len, sizeof(uart2_rx_buf));
    bsp_uart_start_it(&huart2, &uart2_rx_byte);
}

void bsp_uart3_rx_start(void)
{
    bsp_uart_reset_buffer(uart3_rx_buf, &uart3_rx_len, sizeof(uart3_rx_buf));
    bsp_uart_start_it(&huart3, &uart3_rx_byte);
}

void bsp_uart3_rx_restart(void)
{
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_FLAG(&huart3, UART_FLAG_ORE);
    }

    bsp_uart_restart_it(&huart3, &uart3_rx_byte);
}

void bsp_uart1_send_string(const char *str)
{
    if (str == NULL) {
        return;
    }

    bsp_uart_send(&huart1, (const uint8_t *)str, (uint16_t)strlen(str));
}

void bsp_uart2_send_string(const char *str)
{
    if (str == NULL) {
        return;
    }

    bsp_uart_send(&huart2, (const uint8_t *)str, (uint16_t)strlen(str));
}

void bsp_uart3_send_string(const char *str)
{
    if (str == NULL) {
        return;
    }

    bsp_uart_send(&huart3, (const uint8_t *)str, (uint16_t)strlen(str));
}

void bsp_uart1_send_data(const uint8_t *data, uint16_t len)
{
    bsp_uart_send(&huart1, data, len);
}

void bsp_uart2_send_data(const uint8_t *data, uint16_t len)
{
    bsp_uart_send(&huart2, data, len);
}

void bsp_uart3_send_data(const uint8_t *data, uint16_t len)
{
    bsp_uart_send(&huart3, data, len);
}

void bsp_uart_clear_rx_buffer(UART_HandleTypeDef *huart)
{
    if (huart == NULL) {
        return;
    }

    if (huart->Instance == USART1) {
        bsp_uart_reset_buffer(uart1_rx_buf, &uart1_rx_len, sizeof(uart1_rx_buf));
    } else if (huart->Instance == USART2) {
        bsp_uart_reset_buffer(uart2_rx_buf, &uart2_rx_len, sizeof(uart2_rx_buf));
    } else if (huart->Instance == USART3) {
        bsp_uart_reset_buffer(uart3_rx_buf, &uart3_rx_len, sizeof(uart3_rx_buf));
    }
}

uint16_t bsp_uart_copy_rx_buffer(UART_HandleTypeDef *huart, uint8_t *dst, uint16_t dst_size)
{
    if (huart == NULL) {
        return 0U;
    }

    if (huart->Instance == USART1) {
        return bsp_uart_copy_buffer(uart1_rx_buf, &uart1_rx_len, dst, dst_size);
    }
    if (huart->Instance == USART2) {
        return bsp_uart_copy_buffer(uart2_rx_buf, &uart2_rx_len, dst, dst_size);
    }
    if (huart->Instance == USART3) {
        return bsp_uart_copy_buffer(uart3_rx_buf, &uart3_rx_len, dst, dst_size);
    }

    return 0U;
}

uint16_t bsp_uart_get_rx_length(UART_HandleTypeDef *huart)
{
    if (huart == NULL) {
        return 0U;
    }

    if (huart->Instance == USART1) {
        return uart1_rx_len;
    }
    if (huart->Instance == USART2) {
        return uart2_rx_len;
    }
    if (huart->Instance == USART3) {
        return uart3_rx_len;
    }

    return 0U;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == NULL) {
        return;
    }

    if (huart->Instance == USART1) {
        bsp_uart_rx_append(uart1_rx_byte, uart1_rx_buf, &uart1_rx_len, sizeof(uart1_rx_buf));
        bsp_uart_restart_it(&huart1, &uart1_rx_byte);
    } else if (huart->Instance == USART2) {
        bsp_uart_rx_append(uart2_rx_byte, uart2_rx_buf, &uart2_rx_len, sizeof(uart2_rx_buf));
        bsp_uart_restart_it(&huart2, &uart2_rx_byte);
    } else if (huart->Instance == USART3) {
        bsp_uart_rx_append(uart3_rx_byte, uart3_rx_buf, &uart3_rx_len, sizeof(uart3_rx_buf));
        bsp_lcd_uart3_rx_add(1U);
        bsp_uart_restart_it(&huart3, &uart3_rx_byte);
    }
}
