#include "ring.h"
#include "main.h"
#include <string.h>

/* 无锁取模：RING_SIZE 必须是 2 的幂 */
#define RING_MASK (RING_SIZE - 1U)

static inline uint16_t ring_next(uint16_t idx)
{
    return (uint16_t)((idx + 1) & RING_MASK);
}

void ring_init(ring_t *r)
{
    memset(r->buf, 0, sizeof(r->buf));
    r->head = 0;
    r->tail = 0;
}

void ring_put(ring_t *r, uint8_t byte)
{
    uint16_t next_head = ring_next(r->head);

    /* 如果下一个写入位置 == tail，说明满了，丢弃 */
    if (next_head != r->tail) {
        r->buf[r->head] = byte;
        /* 内存屏障：确保 buf 写入在 head 更新之前完成 */
        __DMB();
        r->head = next_head;
    }
}

void ring_put_n(ring_t *r, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        ring_put(r, data[i]);
    }
}

uint8_t ring_get(ring_t *r)
{
    uint8_t val = 0;

    if (r->head != r->tail) {
        /* 内存屏障：确保 head 读取在 buf 读取之前 */
        __DMB();
        val = r->buf[r->tail];
        r->tail = ring_next(r->tail);
    }

    return val;
}

uint16_t ring_get_n(ring_t *r, uint8_t *dst, uint16_t max_len)
{
    uint16_t count = 0;

    while (count < max_len && r->head != r->tail) {
        __DMB();  /* 确保 head 读取在 buf 读取之前 */
        dst[count++] = ring_get(r);
    }

    return count;
}

uint16_t ring_free_count(const ring_t *r)
{
    return (uint16_t)((RING_SIZE - 1) - ring_used_count(r));
}

uint16_t ring_used_count(const ring_t *r)
{
    if (r->head >= r->tail) {
        return (uint16_t)(r->head - r->tail);
    }
    return (uint16_t)(RING_SIZE - (r->tail - r->head));
}

void ring_reset(ring_t *r)
{
    r->head = 0;
    r->tail = 0;
}
