#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "main.h"
#include "usart.h"
#include <stdint.h>

void bsp_uart1_rx_start(void);
void bsp_uart2_rx_start(void);
void bsp_uart3_rx_start(void);
void bsp_uart3_rx_restart(void);

void bsp_uart1_send_string(const char *str);
void bsp_uart2_send_string(const char *str);
void bsp_uart3_send_string(const char *str);
void bsp_uart1_send_data(const uint8_t *data, uint16_t len);
void bsp_uart2_send_data(const uint8_t *data, uint16_t len);
void bsp_uart3_send_data(const uint8_t *data, uint16_t len);

void bsp_uart_clear_rx_buffer(UART_HandleTypeDef *huart);
uint16_t bsp_uart_copy_rx_buffer(UART_HandleTypeDef *huart, uint8_t *dst, uint16_t dst_size);
uint16_t bsp_uart_get_rx_length(UART_HandleTypeDef *huart);

extern uint8_t uart1_rx_buf[256];
extern volatile uint16_t uart1_rx_len;
extern uint8_t uart2_rx_buf[256];
extern volatile uint16_t uart2_rx_len;
extern uint8_t uart3_rx_buf[256];
extern volatile uint16_t uart3_rx_len;

#endif
