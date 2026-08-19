#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>

/* 滤波窗口大小：16 点滑动平均，平衡响应速度和噪声抑制 */
#define FILTER_WINDOW_SIZE  16

/* 中值滤波触发阈值：滑动平均值与中值偏差超过 30% 时用中值替代 */
#define FILTER_MEDIAN_RATIO  0.30f

/* 单通道滤波器状态 */
typedef struct {
    uint16_t window[FILTER_WINDOW_SIZE];  /* 滑动窗口 */
    uint8_t  index;                       /* 当前写入位置 */
    uint8_t  filled;                      /* 窗口是否已满（前 FILTER_WINDOW_SIZE 次不完整） */
    uint32_t sum;                         /* 窗口内累加值（快速滑动平均） */
    uint16_t avg;                         /* 当前滑动平均值 */
} filter_ch_t;

/* 初始化一个滤波器通道 */
void filter_init(filter_ch_t *f);

/* 喂入一个原始值，返回滤波后的值
   先做滑动平均，再与中值比较，偏差过大则用中值替代 */
uint16_t filter_update(filter_ch_t *f, uint16_t raw);

/* 获取当前滑动平均值 */
uint16_t filter_get_avg(const filter_ch_t *f);

#endif
