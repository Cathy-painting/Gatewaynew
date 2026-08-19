#ifndef BSP_ADC_H
#define BSP_ADC_H

#include "main.h"
#include <stdint.h>

/* ADC 通道序号（与 terminal_data.h 保持一致） */
#define BSP_ADC_CH_PB15  0U     /* PB15 = ADC2_IN15, R37 电位器 */
#define BSP_ADC_CH_PB12  1U     /* PB12 = ADC2_IN12, R38 电位器 */
#define BSP_ADC_CH_NTC   2U     /* PA0  = ADC2_IN1,  预留 NTC  */
#define BSP_ADC_CH_NUM   3U

/* DMA 循环缓冲区：3 通道 × 连续采样 */
#define BSP_ADC_DMA_BUF_SIZE  (BSP_ADC_CH_NUM * 8)  /* 每点触发一次转换完成，每个通道存 8 个样本 */

/* 初始化 ADC2 为 3 通道 DMA 连续扫描模式
   必须在系统时钟和 GPIO 初始化后调用，在 FreeRTOS 调度器启动前调用 */
void bsp_adc_init(void);

/* 启动 DMA 连续转换 */
void bsp_adc_start(void);

/* 停止 DMA 转换 */
void bsp_adc_stop(void);

/* 读取 DMA 缓冲区中指定通道的最新样本
   返回 0-4095 的 ADC 原始值 */
uint16_t bsp_adc_get_raw(uint8_t ch);

/* 转换 ADC 原始值到电压（V） */
static inline float bsp_adc_to_voltage(uint16_t raw)
{
    return (float)raw / 4095.0f * 3.3f;
}

#endif
