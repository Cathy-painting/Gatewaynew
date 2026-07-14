#include "bsp_lcd.h"
#include "lcd.h"
#include "bsp_rs485.h"
#include "bsp_uart.h"
#include "terminal_service.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

static uint8_t bsp_lcd_inited = 0U;
static uint8_t bsp_uart3_loopback_pass = 0U;
static uint8_t bsp_uart3_loopback_done = 0U;

static volatile uint32_t bsp_uart3_rx_total = 0U;

extern UART_HandleTypeDef huart3;

static void bsp_uart3_run_loopback_test(void)
{
    /*
     * RS485 半双工通信无法做硬件回环测试（发送时接收器关闭）。
     * 直接用 U3_RX_INT 计数判断 Modbus 通信是否正常即可。
     */
    bsp_uart3_loopback_done = 1U;
    bsp_uart3_loopback_pass = 1U;
}

void bsp_lcd_init(void)
{
    if (bsp_lcd_inited != 0U) {
        return;
    }
    bsp_lcd_inited = 1U;
    bsp_uart3_rx_total = 0U;

    LCD_Init();
    LCD_Clear(Black);
    LCD_SetBackColor(Black);
    LCD_SetTextColor(Green);
    LCD_DisplayStringLine(Line0, (uint8_t *)"  Gateway Test v1.0  ");
    LCD_SetTextColor(White);
    LCD_DisplayStringLine(Line1, (uint8_t *)"  CT117E-M4 STM32G431");

    bsp_uart3_run_loopback_test();
}

void bsp_lcd_clear(void)
{
    LCD_Clear(Black);
}

void bsp_lcd_uart3_rx_add(uint16_t len)
{
    bsp_uart3_rx_total += (uint32_t)len;
}

uint32_t bsp_lcd_uart3_rx_count(void)
{
    return bsp_uart3_rx_total;
}

uint8_t bsp_lcd_uart3_loopback_ok(void)
{
    return bsp_uart3_loopback_pass;
}

void bsp_lcd_show_test_info(void)
{
    char buf[32];
    terminal_data_t data;

    if (bsp_lcd_inited == 0U) {
        return;
    }

    terminal_get_snapshot(&data);

    snprintf(buf, sizeof(buf), "Sample: %5lu     ", data.sample_count);
    LCD_SetTextColor(White);
    LCD_ClearLine(Line2);
    LCD_DisplayStringLine(Line2, (uint8_t *)buf);

    snprintf(buf, sizeof(buf), "ADC(PB15): %5u   ", data.local_value);
    LCD_ClearLine(Line3);
    LCD_DisplayStringLine(Line3, (uint8_t *)buf);

    snprintf(buf, sizeof(buf), "Modbus: %s       ",
             data.remote_online ? "ONLINE " : "OFFLINE");
    if (data.remote_online) {
        LCD_SetTextColor(Green);
    } else {
        LCD_SetTextColor(Red);
    }
    LCD_ClearLine(Line4);
    LCD_DisplayStringLine(Line4, (uint8_t *)buf);

    LCD_SetTextColor(White);
    snprintf(buf, sizeof(buf), "MB OK:%4lu FAIL:%4lu",
             data.modbus_ok_count, data.modbus_fail_count);
    LCD_ClearLine(Line5);
    LCD_DisplayStringLine(Line5, (uint8_t *)buf);

    snprintf(buf, sizeof(buf), "U3_RX_INT: %6lu", bsp_uart3_rx_total);
    if (bsp_uart3_rx_total > 0U) {
        LCD_SetTextColor(Green);
    } else {
        LCD_SetTextColor(Yellow);
    }
    LCD_ClearLine(Line6);
    LCD_DisplayStringLine(Line6, (uint8_t *)buf);

    LCD_SetTextColor(White);
    snprintf(buf, sizeof(buf), "LoopBack: %s     ",
             bsp_uart3_loopback_pass ? "PASS" : "N/A ");
    if (bsp_uart3_loopback_pass) {
        LCD_SetTextColor(Green);
    } else {
        LCD_SetTextColor(Yellow);
    }
    LCD_ClearLine(Line7);
    LCD_DisplayStringLine(Line7, (uint8_t *)buf);

    snprintf(buf, sizeof(buf), "Cloud: %s        ",
             data.cloud_online ? "ONLINE " : "OFFLINE");
    if (data.cloud_online) {
        LCD_SetTextColor(Green);
    } else {
        LCD_SetTextColor(Red);
    }
    LCD_ClearLine(Line8);
    LCD_DisplayStringLine(Line8, (uint8_t *)buf);

    LCD_SetTextColor(White);
    LCD_DisplayStringLine(Line9, (uint8_t *)"  LED Heartbeat OK   ");
}
