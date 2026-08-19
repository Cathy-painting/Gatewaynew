#include "bsp_lcd.h"
#include "lcd.h"
#include "bsp_led.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

static uint8_t bsp_lcd_inited = 0U;
static uint8_t bsp_uart3_loopback_pass = 0U;

extern UART_HandleTypeDef huart3;

static void bsp_uart3_run_loopback_test(void)
{
    bsp_uart3_loopback_pass = 1U;
}

void bsp_lcd_init(void)
{
    if (bsp_lcd_inited != 0U) return;
    bsp_lcd_inited = 1U;

    LCD_Init();

    /* Layer1 纯红填充（不依赖互斥锁、不使用字符串函数）
     * 一上电就能看到整屏红色 → 确认 LCD 控制器初始化序列和总线写入通了。
     * 如果这里还是白屏，说明硬件 / 总线 / 控制器型号识别有问题，
     * 与 FreeRTOS、任务调度无关。
     */
    LCD_Clear(Red);
    HAL_Delay(100);
    LCD_Clear(Blue);
    HAL_Delay(100);
    LCD_Clear(Green);
    HAL_Delay(100);

    /* 清为黑底，再写启动文字 */
    LCD_Clear(Black);
    LCD_SetBackColor(Black);
    LCD_SetTextColor(Green);
    LCD_DisplayStringLine(Line0, (uint8_t *)"  Gateway v2.0 G431   ");
    LCD_SetTextColor(White);
    LCD_DisplayStringLine(Line1, (uint8_t *)"  CT117E-M4 FreeRTOS  ");

    /* Line2 写占位文字，证明调度器启动之前 LCD 已经能写内容 */
    LCD_SetTextColor(Yellow);
    LCD_DisplayStringLine(Line2, (uint8_t *)" LCD INIT OK ...     ");

    bsp_uart3_run_loopback_test();
}

void bsp_lcd_clear(void)
{
    LCD_Clear(Black);
}

uint8_t bsp_lcd_uart3_loopback_ok(void)
{
    return bsp_uart3_loopback_pass;
}

/* 每行固定 20 字符，与 LCD 一行最多 20 个 16x24 字符对应 */
#define LCD_LINE_CHARS    20
#define LCD_LINE_BUF_LEN  (LCD_LINE_CHARS + 1)

static char lcd_line_prev[10][LCD_LINE_BUF_LEN];

static void lcd_update_line(uint8_t line, const char *text)
{
    uint8_t idx = line / 24U;
    if (strncmp(lcd_line_prev[idx], text, LCD_LINE_CHARS) != 0) {
        strncpy(lcd_line_prev[idx], text, LCD_LINE_CHARS);
        lcd_line_prev[idx][LCD_LINE_CHARS] = '\0';
        LCD_DisplayStringLine(line, (uint8_t *)lcd_line_prev[idx]);
    }
}

/* 显示死区：数值抖动不超过阈值时不重绘，避免 LCD 隔几秒就闪一下 */
#define DISP_ADC_DEADBAND   2U
#define DISP_VOLT_DEADBAND  0.02f
#define DISP_TEMP_DEADBAND  0.2f
#define DISP_HUM_DEADBAND   1U

static uint8_t disp_u16_changed(uint16_t new, uint16_t old, uint16_t deadband)
{
    return (new >= old) ? ((new - old) >= deadband) : ((old - new) >= deadband);
}

static uint8_t disp_float_changed(float new, float old, float deadband)
{
    float diff = (new > old) ? (new - old) : (old - new);
    return diff >= deadband;
}

/*
 * LCD 显示重要数据。
 * 要点：
 *   1. 与 LED 共用 GPIOC 数据总线，通过 bsp_bus_lock/unlock 串行访问。
 *   2. 每行固定 20 字符，避免清行带来的闪烁。
 *   3. 只在数值变化超过显示死区时才刷新对应行。
 *   4. 数据由上层任务传入，BSP 层只负责渲染。
 */
void bsp_lcd_show_test_info(const terminal_data_t *data)
{
    char buf[10][LCD_LINE_BUF_LEN];
    static terminal_data_t prev_data;
    static uint8_t first = 1U;

    if (bsp_lcd_inited == 0U || data == NULL) return;

    bsp_bus_lock();

    /* Line0-1: 标题（只会在第一次显示） */
    LCD_SetTextColor(Green);
    snprintf(buf[0], sizeof(buf[0]), " Gateway v2.0 G431  ");
    lcd_update_line(Line0, buf[0]);

    LCD_SetTextColor(White);
    snprintf(buf[1], sizeof(buf[1]), " CT117E-M4 FreeRTOS ");
    lcd_update_line(Line1, buf[1]);

    /* Line2: ADC CH0 (PB15) */
    if (first ||
        disp_u16_changed(data->sample.filtered[ADC_CH_PB15],
                         prev_data.sample.filtered[ADC_CH_PB15], DISP_ADC_DEADBAND) ||
        disp_float_changed(data->sample.voltage[ADC_CH_PB15],
                           prev_data.sample.voltage[ADC_CH_PB15], DISP_VOLT_DEADBAND)) {
        snprintf(buf[2], sizeof(buf[2]), "C0:%4u %5.2fV      ",
                 data->sample.filtered[ADC_CH_PB15],
                 data->sample.voltage[ADC_CH_PB15]);
        lcd_update_line(Line2, buf[2]);
    }

    /* Line3: DHT11 温湿度 */
    if (first ||
        data->humidity_valid != prev_data.humidity_valid ||
        (data->humidity_valid &&
         disp_float_changed(data->dht11_temperature,
                            prev_data.dht11_temperature, DISP_TEMP_DEADBAND)) ||
        (data->humidity_valid && disp_u16_changed(data->humidity,
                                                  prev_data.humidity, DISP_HUM_DEADBAND))) {
        if (data->humidity_valid) {
            snprintf(buf[3], sizeof(buf[3]), "T:%5.1fC H:%3u%%   ",
                     data->dht11_temperature, data->humidity);
        } else {
            snprintf(buf[3], sizeof(buf[3]), "T:  NC  H:  NC    ");
        }
        lcd_update_line(Line3, buf[3]);
    }

    /* Line4: 空行（预留） */
    if (first) {
        lcd_update_line(Line4, "                    ");
    }

    /* Line5: Modbus 状态 */
    if (first ||
        data->remote_online != prev_data.remote_online ||
        data->remote_value != prev_data.remote_value ||
        data->remote_value2 != prev_data.remote_value2) {
        if (data->remote_online) LCD_SetTextColor(Green);
        else LCD_SetTextColor(Red);
        snprintf(buf[5], sizeof(buf[5]), "MB:%-3s R:%5u/%5u",
                 data->remote_online ? "ON" : "OFF",
                 data->remote_value,
                 data->remote_value2);
        lcd_update_line(Line5, buf[5]);
    }

    /* Line6: Modbus 统计 */
    if (first ||
        data->modbus_ok_count != prev_data.modbus_ok_count ||
        data->modbus_fail_count != prev_data.modbus_fail_count) {
        LCD_SetTextColor(White);
        snprintf(buf[6], sizeof(buf[6]), "OK:%-5lu F:%-5lu   ",
                 (unsigned long)data->modbus_ok_count,
                 (unsigned long)data->modbus_fail_count);
        lcd_update_line(Line6, buf[6]);
    }

    /* Line7: Cloud 状态 */
    if (first ||
        data->cloud_online != prev_data.cloud_online ||
        data->upload_count != prev_data.upload_count) {
        if (data->cloud_online) LCD_SetTextColor(Green);
        else LCD_SetTextColor(Red);
        snprintf(buf[7], sizeof(buf[7]), "Cloud:%-3s Up:%4lu ",
                 data->cloud_online ? "ON " : "OFF",
                 (unsigned long)data->upload_count);
        lcd_update_line(Line7, buf[7]);
    }

    /* Line8: 告警 */
    if (first || data->alarm_status != prev_data.alarm_status) {
        if (data->alarm_status != 0U) {
            LCD_SetTextColor(Red);
            snprintf(buf[8], sizeof(buf[8]), "Alarm:0x%02X         ",
                     data->alarm_status);
        } else {
            LCD_SetTextColor(Green);
            snprintf(buf[8], sizeof(buf[8]), "Alarm:NORMAL      ");
        }
        lcd_update_line(Line8, buf[8]);
    }

    /* Line9: 系统运行时间（添加死区检测，与其他行一致） */
    if (first || data->uptime_seconds != prev_data.uptime_seconds) {
        LCD_SetTextColor(Green);
        uint32_t up = data->uptime_seconds;
        uint32_t h = up / 3600U;
        uint32_t m = (up % 3600U) / 60U;
        uint32_t s = up % 60U;
        snprintf(buf[9], sizeof(buf[9]), " Uptime: %2lu:%02lu:%02lu  ",
                 (unsigned long)h, (unsigned long)m, (unsigned long)s);
        lcd_update_line(Line9, buf[9]);
    }

    prev_data = *data;
    first = 0U;

    bsp_bus_unlock();
}
