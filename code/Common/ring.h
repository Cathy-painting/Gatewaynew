#ifndef RING_H
#define RING_H

#include <stdint.h>

/* 环形缓冲区：无锁 SPSC（Single Producer Single Consumer）
   生产者（ISR/DMA 回调）调用 ring_put，消费者（任务）调用 ring_get
   size 必须是 2 的幂，用 & mask 替代 % 取模 */

#define RING_SIZE 256U  /* 必须是 2 的幂 */

typedef struct {
    uint8_t          buf[RING_SIZE];
    volatile uint16_t head;    /* 生产者写入位置（ISR 中递增） */
    volatile uint16_t tail;    /* 消费者读取位置（任务中递增） */
} ring_t;

/* 初始化环形缓冲区 */
void ring_init(ring_t *r);

/* 生产者：放入一个字节，满则丢弃 */
void ring_put(ring_t *r, uint8_t byte);

/* 生产者：放入多个字节 */
void ring_put_n(ring_t *r, const uint8_t *data, uint16_t len);

/* 消费者：读取一个字节，空返回 0 */
uint8_t ring_get(ring_t *r);

/* 消费者：读取多个字节，返回实际读取数 */
uint16_t ring_get_n(ring_t *r, uint8_t *dst, uint16_t max_len);

/* 生产者：获取可写入空间 */
uint16_t ring_free_count(const ring_t *r);

/* 消费者：获取可读取数据量 */
uint16_t ring_used_count(const ring_t *r);

/* 清空缓冲区 */
void ring_reset(ring_t *r);

#endif
