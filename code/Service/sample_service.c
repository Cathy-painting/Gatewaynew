// sample_service.c - 后厨：处理采样和串口业务逻辑
#include "sample_service.h"
#include "bsp_adc.h"
#include "bsp_uart.h"
#include "terminal_service.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

// 外部变量声明（来自 bsp_uart.c）
extern uint8_t uart3_rx_buf[256];
extern uint16_t uart3_rx_len;
extern uint8_t uart2_rx_buf[256];
extern uint16_t uart2_rx_len;

uint16_t sample_service_read_local(void)
{
    return bsp_adc_read();
}

// 业务逻辑：采样、串口接收处理
void sample_service_process(void)
{
    uint16_t adc_value;
    char buf[64];
    
    // === 第一部分：ADC采样与记录 ===
    adc_value = bsp_adc_read();
    terminal_set_local_value(adc_value);
    snprintf(buf, sizeof(buf), "[SAMPLE] local=%u\r\n", adc_value);
    log_info(buf);

    // === 第二部分：安全地处理USART3接收的数据 ===
    {
        uint16_t len_copy = 0;
        uint8_t buf_copy[256] = {0};
        
        __disable_irq();
        len_copy = uart3_rx_len;
        if (len_copy > 0 && len_copy <= sizeof(buf_copy)) {
            memcpy(buf_copy, uart3_rx_buf, len_copy);
            uart3_rx_len = 0;
        }
        __enable_irq();
        
        if (len_copy > 0) {
            snprintf(buf, sizeof(buf), "[USART3 RX] len=%d: ", len_copy);
            log_info(buf);
            for (int i = 0; i < len_copy && i < 20; i++) {
                snprintf(buf, sizeof(buf), "%02X ", buf_copy[i]);
                log_info(buf);
            }
            log_info("\r\n");
        }
    }

    // === 第三部分：安全地处理USART2接收的数据 (与USART3逻辑完全相同) ===
    {
        uint16_t len_copy = 0;
        uint8_t buf_copy[256] = {0};
        
        __disable_irq();
        len_copy = uart2_rx_len;
        if (len_copy > 0 && len_copy <= sizeof(buf_copy)) {
            memcpy(buf_copy, uart2_rx_buf, len_copy);
            uart2_rx_len = 0;
        }
        __enable_irq();
        
        if (len_copy > 0) {
            snprintf(buf, sizeof(buf), "[USART2 RX] len=%d: ", len_copy);
            log_info(buf);
            for (int i = 0; i < len_copy && i < 20; i++) {
                snprintf(buf, sizeof(buf), "%02X ", buf_copy[i]);
                log_info(buf);
            }
            log_info("\r\n");
        }
    }

    // 第八批阶段：USART3 仅作为被测接收口，避免发送干扰验证
}
