#ifndef TERMINAL_DATA_H
#define TERMINAL_DATA_H

#include <stdint.h>

/* ADC 通道宏定义：板载 2 个旋钮 + 1 个预留 NTC 通道 */
#define ADC_CH_NUM      3U
#define ADC_CH_PB15     0U      /* J11/R37 电位器，PB15 */
#define ADC_CH_PB12     1U      /* J12/R38 电位器，PB12 */
#define ADC_CH_NTC      2U      /* 预留：外接 NTC 传感器，PA0 */

/* 告警位图 */
#define ALARM_BIT_OVER_TEMP     (1U << 0)
#define ALARM_BIT_UNDER_TEMP    (1U << 1)
#define ALARM_BIT_OVER_VOLT     (1U << 2)
#define ALARM_BIT_UNDER_VOLT    (1U << 3)

/* 告警消抖：连续越限多少次才置位 */
#define ALARM_DEBOUNCE_CNT      3U

typedef struct {
    /* 多通道 ADC 采样 */
    struct {
        uint16_t raw[ADC_CH_NUM];       /* 各通道原始 ADC 值（0-4095） */
        uint16_t filtered[ADC_CH_NUM];  /* 滤波后 ADC 值 */
        float    voltage[ADC_CH_NUM];   /* 电压值（V），通道2为 NTC 分压 */
        float    temperature[ADC_CH_NUM]; /* 温度值（℃），仅通道2有效 */
    } sample;

    /* Modbus 远程数据 */
    uint16_t remote_value;
    uint16_t remote_value2;
    uint8_t  remote_online;

    /* 云端连接状态 */
    uint8_t  cloud_online;

    /* 统计计数 */
    uint32_t sample_count;
    uint32_t modbus_ok_count;
    uint32_t modbus_fail_count;
    uint32_t upload_count;

    /* DHT11 温湿度 */
    uint8_t  humidity;              /* 湿度 %RH */
    uint8_t  humidity_valid;        /* 传感器数据有效 */
    float    dht11_temperature;     /* DHT11 温度（℃），不覆盖 NTC 通道数据 */

    /* 告警 */
    uint8_t  alarm_status;          /* 当前告警位图 */
    uint8_t  alarm_debounce[4];     /* 各告警的消抖计数器 */

    /* 兼容旧代码：local_value 指向 sample.filtered[0] -- 通过 terminal_get_local_value() 获取 */
    uint16_t local_value;

    /* 系统运行时间 */
    uint32_t uptime_seconds;
} terminal_data_t;

#endif
