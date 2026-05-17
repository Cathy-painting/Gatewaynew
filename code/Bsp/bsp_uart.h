#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "main.h"
#include "usart.h"
#include <stdint.h>
#include <string.h>

void bsp_uart_start_receive(void);
void bsp_uart1_send_string(const char *str);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void bsp_uart_data_process(void);

#endif
