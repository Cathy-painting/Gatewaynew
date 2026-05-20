#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "main.h"
#include "usart.h"
#include <stdint.h>
#include <string.h>

void bsp_uart3_rx_start(void);
void bsp_uart2_rx_start(void);


void bsp_uart1_start_receive(void);
void bsp_uart2_start_receive(void);
void bsp_uart3_start_receive(void);

void bsp_uart1_send_string(const char *str);
void bsp_uart2_send_string(const char *str);
void bsp_uart3_send_string(const char *str);

extern uint8_t uart3_rx_buf[];
extern uint16_t uart3_rx_len;
extern uint8_t uart3_rx_byte;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

#endif
