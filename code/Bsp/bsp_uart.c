#include "bsp_uart.h"
#include "main.h"
#include <string.h>

/* ========================================================================
   串口模块 v2：中断接收 + 环形缓冲区 + DMA 发送

   改进：
   1. USART2/USART3 接收使用环形缓冲区（head/tail 双指针），不再丢数据
   2. USART1 保持简单线性 buffer（日志专用）
   3. API 向后兼容
   ======================================================================== */

/* --- USART1 日志（线性 buffer） --- */
#define UART1_BUF_SIZE  128U
uint8_t      uart1_rx_buf[UART1_BUF_SIZE];
volatile uint16_t uart1_rx_len = 0;
static uint8_t uart1_rx_byte = 0;

/* --- USART2 ESP32（环形缓冲区）--- */
#define UART2_DMA_BUF_SIZE  256U
static ring_t uart2_ring;
static uint8_t uart2_rx_byte = 0;

/* --- USART3 Modbus（环形缓冲区）--- */
#define UART3_DMA_BUF_SIZE  256U
static ring_t uart3_ring;
static uint8_t uart3_rx_byte = 0;

/* ========================================================================
   发送：DMA 非阻塞发送
   ======================================================================== */
static void bsp_uart_send_blocking(UART_HandleTypeDef *huart, const uint8_t *data, uint16_t len)
{
    if (huart == NULL || data == NULL || len == 0U) return;
    HAL_UART_Transmit(huart, (uint8_t *)data, len, UART_TX_TIMEOUT_MS);
}

/* ========================================================================
   接收中断公共方法
   ======================================================================== */
static void bsp_uart_start_it(UART_HandleTypeDef *huart, uint8_t *byte)
{
    if (huart == NULL || byte == NULL) return;
    HAL_UART_Receive_IT(huart, byte, 1);
}

static void bsp_uart_restart_it(UART_HandleTypeDef *huart, uint8_t *byte)
{
    if (huart == NULL || byte == NULL) return;

    if (HAL_UART_Receive_IT(huart, byte, 1) != HAL_OK) {
        /* HALT 硬件恢复：手动重置接收状态 */
        huart->pRxBuffPtr  = byte;
        huart->RxXferSize  = 1U;
        huart->RxXferCount = 1U;
        huart->RxState     = HAL_UART_STATE_BUSY_RX;
        __HAL_UART_ENABLE_IT(huart, UART_IT_RXNE);
    }
}

/* ========================================================================
   USART1 日志串口
   ======================================================================== */
void bsp_uart1_rx_start(void)
{
    uart1_rx_len = 0;
    memset(uart1_rx_buf, 0, sizeof(uart1_rx_buf));
    bsp_uart_start_it(&huart1, &uart1_rx_byte);
}

void bsp_uart1_send_string(const char *str)
{
    if (str == NULL) return;
    bsp_uart_send_blocking(&huart1, (const uint8_t *)str, (uint16_t)strlen(str));
}

void bsp_uart1_send_data(const uint8_t *data, uint16_t len)
{
    bsp_uart_send_blocking(&huart1, data, len);
}

/* ========================================================================
   USART2 ESP32（环形缓冲区）
   ======================================================================== */
void bsp_uart2_rx_start(void)
{
    ring_reset(&uart2_ring);
    bsp_uart_start_it(&huart2, &uart2_rx_byte);
}

void bsp_uart2_send_string(const char *str)
{
    if (str == NULL) return;
    bsp_uart_send_blocking(&huart2, (const uint8_t *)str, (uint16_t)strlen(str));
}

void bsp_uart2_send_data(const uint8_t *data, uint16_t len)
{
    bsp_uart_send_blocking(&huart2, data, len);
}

/* ========================================================================
   USART3 Modbus（环形缓冲区）
   ======================================================================== */
void bsp_uart3_rx_start(void)
{
    ring_reset(&uart3_ring);
    bsp_uart_start_it(&huart3, &uart3_rx_byte);
}

void bsp_uart3_rx_restart(void)
{
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_FLAG(&huart3, UART_FLAG_ORE);
    }
    bsp_uart_restart_it(&huart3, &uart3_rx_byte);
}

void bsp_uart3_send_string(const char *str)
{
    if (str == NULL) return;
    bsp_uart_send_blocking(&huart3, (const uint8_t *)str, (uint16_t)strlen(str));
}

void bsp_uart3_send_data(const uint8_t *data, uint16_t len)
{
    bsp_uart_send_blocking(&huart3, data, len);
}

/* ========================================================================
   通用操作：clear/copy/get_length（兼容旧 API）
   USART2/USART3 使用环形缓冲区，USART1 使用线性 buffer
   ======================================================================== */

void bsp_uart_clear_rx_buffer(UART_HandleTypeDef *huart)
{
    if (huart == NULL) return;

    if (huart->Instance == USART1) {
        __disable_irq();
        uart1_rx_len = 0;
        memset(uart1_rx_buf, 0, sizeof(uart1_rx_buf));
        __enable_irq();
    } else if (huart->Instance == USART2) {
        ring_reset(&uart2_ring);
    } else if (huart->Instance == USART3) {
        ring_reset(&uart3_ring);
    }
}

uint16_t bsp_uart_copy_rx_buffer(UART_HandleTypeDef *huart, uint8_t *dst, uint16_t dst_size)
{
    if (huart == NULL || dst == NULL || dst_size == 0U) return 0U;

    if (huart->Instance == USART1) {
        uint16_t copy_len;
        __disable_irq();
        copy_len = uart1_rx_len;
        if (copy_len > dst_size) copy_len = dst_size;
        if (copy_len > 0U) {
            memcpy(dst, uart1_rx_buf, copy_len);
            uart1_rx_len = 0;
        }
        __enable_irq();
        return copy_len;
    }

    if (huart->Instance == USART2) {
        return ring_get_n(&uart2_ring, dst, dst_size);
    }

    if (huart->Instance == USART3) {
        return ring_get_n(&uart3_ring, dst, dst_size);
    }

    return 0U;
}

uint16_t bsp_uart_get_rx_length(UART_HandleTypeDef *huart)
{
    if (huart == NULL) return 0U;

    if (huart->Instance == USART1) {
        return uart1_rx_len;
    }
    if (huart->Instance == USART2) {
        return ring_used_count(&uart2_ring);
    }
    if (huart->Instance == USART3) {
        return ring_used_count(&uart3_ring);
    }
    return 0U;
}

/* ========================================================================
   HAL 中断回调（3 个 UART 共用）
   ======================================================================== */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == NULL) return;

    if (huart->Instance == USART1) {
        /* USART1：线性追加 */
        if (uart1_rx_len < (UART1_BUF_SIZE - 1U)) {
            uart1_rx_buf[uart1_rx_len++] = uart1_rx_byte;
        }
        bsp_uart_restart_it(&huart1, &uart1_rx_byte);
    }
    else if (huart->Instance == USART2) {
        /* USART2：环形缓冲区 */
        ring_put(&uart2_ring, uart2_rx_byte);
        bsp_uart_restart_it(&huart2, &uart2_rx_byte);
    }
    else if (huart->Instance == USART3) {
        /* USART3：环形缓冲区 */
        ring_put(&uart3_ring, uart3_rx_byte);
        bsp_uart_restart_it(&huart3, &uart3_rx_byte);
    }
}
