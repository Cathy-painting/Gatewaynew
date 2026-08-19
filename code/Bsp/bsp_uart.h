#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "main.h"
#include "usart.h"
#include "ring.h"
#include <stdint.h>

/* ========================================================================
   串口模块
   USART1 (PA9/PA10): 调试日志输出，仅发送，不使用 DMA
   USART2 (PA2/PA3):  ESP32 双向通信，DMA+空闲中断接收，DMA 发送
   USART3 (PB10/PB11): RS485/Modbus，DMA+空闲中断接收，DMA 发送
   ======================================================================== */

/* UART 发送超时（ms），避免持有互斥锁时长时间阻塞 */
#define UART_TX_TIMEOUT_MS  100U

/* --- USART1 日志串口（简单模式）--- */
void bsp_uart1_rx_start(void);
void bsp_uart1_send_string(const char *str);
void bsp_uart1_send_data(const uint8_t *data, uint16_t len);

/* --- USART2 ESP32（DMA 模式）--- */
void bsp_uart2_init_dma(void);             /* 初始化 DMA+空闲中断 */
void bsp_uart2_rx_start(void);             /* 启动 DMA 接收 */
void bsp_uart2_send_string(const char *str);
void bsp_uart2_send_data(const uint8_t *data, uint16_t len);

/* --- USART3 Modbus（DMA 模式）--- */
void bsp_uart3_init_dma(void);             /* 初始化 DMA+空闲中断 */
void bsp_uart3_rx_start(void);             /* 启动 DMA 接收 */
void bsp_uart3_rx_restart(void);           /* 重新启动 DMA 接收 */
void bsp_uart3_send_string(const char *str);
void bsp_uart3_send_data(const uint8_t *data, uint16_t len);

/* --- 通用操作 --- */
void bsp_uart_clear_rx_buffer(UART_HandleTypeDef *huart);
uint16_t bsp_uart_copy_rx_buffer(UART_HandleTypeDef *huart, uint8_t *dst, uint16_t dst_size);
uint16_t bsp_uart_get_rx_length(UART_HandleTypeDef *huart);

/* --- USART1 兼容（旧代码可能用到）--- */
extern uint8_t uart1_rx_buf[128];
extern volatile uint16_t uart1_rx_len;

#endif
