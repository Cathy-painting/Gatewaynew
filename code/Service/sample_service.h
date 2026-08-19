#ifndef SAMPLE_SERVICE_H
#define SAMPLE_SERVICE_H

#include <stdint.h>

/* 采样服务初始化（调用 bsp_adc_init + bsp_adc_start） */
void sample_service_init(void);

/* 单次采样处理：从 DMA 缓冲区读各通道最新值，滤波后写入 terminal_data */
void sample_service_process(void);

/* 兼容旧接口 */
uint16_t sample_service_read_local(void);

#endif
