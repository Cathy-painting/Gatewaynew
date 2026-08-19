#include "sample_service.h"
#include "bsp_adc.h"
#include "terminal_service.h"
#include "ntc_service.h"
#include "filter.h"
#include "dht11_service.h"
#include "log.h"
#include <stdio.h>

/* 每个 ADC 通道一个独立的滤波器 */
static filter_ch_t g_filter[BSP_ADC_CH_NUM];
static uint8_t    g_ntc_connected = 0U;

void sample_service_init(void)
{
    /* 初始化滤波器 */
    for (uint8_t i = 0; i < BSP_ADC_CH_NUM; i++) {
        filter_init(&g_filter[i]);
    }

    /* 初始化 ADC（DMA 多通道扫描模式） */
    bsp_adc_init();

    /* 启动 DMA 连续采样 */
    bsp_adc_start();

    log_info("[SAMPLE] service init OK\r\n");
}

/* 兼容旧代码 */
uint16_t sample_service_read_local(void)
{
    return bsp_adc_get_raw(BSP_ADC_CH_PB15);
}

void sample_service_process(void)
{
    uint16_t raw[ADC_CH_NUM];
    uint16_t filtered[ADC_CH_NUM];
    float    voltage[ADC_CH_NUM];
    float    temperature[ADC_CH_NUM];

    /* 1. 从 DMA 缓冲区读取各通道最新原始值（零 CPU 等待） */
    raw[0] = bsp_adc_get_raw(BSP_ADC_CH_PB15);
    raw[1] = bsp_adc_get_raw(BSP_ADC_CH_PB12);
    raw[2] = bsp_adc_get_raw(BSP_ADC_CH_NTC);

    /* 2. 滤波：滑动平均 + 中值滤波组合 */
    filtered[0] = filter_update(&g_filter[0], raw[0]);
    filtered[1] = filter_update(&g_filter[1], raw[1]);
    filtered[2] = filter_update(&g_filter[2], raw[2]);

    /* 3. 电压换算 */
    voltage[0] = bsp_adc_to_voltage(filtered[0]);
    voltage[1] = bsp_adc_to_voltage(filtered[1]);
    voltage[2] = bsp_adc_to_voltage(filtered[2]);

    /* 4. NTC 温度采集（通道2） */
    temperature[0] = 0.0f;
    temperature[1] = 0.0f;
    g_ntc_connected = ntc_is_connected(raw[2]);
    if (g_ntc_connected) {
        temperature[2] = ntc_convert_temperature(raw[2]);
    } else {
        temperature[2] = -273.15f;  /* 无效温度 */
    }

    /* 5. 写入 terminal_data */
    for (uint8_t ch = 0; ch < ADC_CH_NUM; ch++) {
        terminal_update_sample(ch, raw[ch], filtered[ch], voltage[ch], temperature[ch]);
    }

    /* 6. 告警检测 */
    terminal_update_alarms();

    /* 7. DHT11 温湿度采集（每次采样周期读一次） */
    dht11_service_read();

    /* 8. 递增采样计数 */
    terminal_inc_sample_count();
    log_infof("[SAMPLE] ch0=%u(%u) %.2fV  ch1=%u(%u) %.2fV  ch2(NTC)=%u(%u) %.1fC %s\r\n",
              raw[0], filtered[0], voltage[0],
              raw[1], filtered[1], voltage[1],
              raw[2], filtered[2], temperature[2],
              g_ntc_connected ? "ONLINE" : "NC");
}
