#include "filter.h"
#include <string.h>

static uint16_t filter_median(const uint16_t *window, uint8_t count)
{
    uint16_t sorted[FILTER_WINDOW_SIZE];
    uint8_t i, j;

    if (count == 0U) {
        return 0U;
    }
    if (count > FILTER_WINDOW_SIZE) {
        count = FILTER_WINDOW_SIZE;
    }

    memcpy(sorted, window, (size_t)count * sizeof(uint16_t));

    for (i = 0U; i < count - 1U; i++) {
        for (j = 0U; j < count - 1U - i; j++) {
            if (sorted[j] > sorted[j + 1U]) {
                uint16_t tmp = sorted[j];
                sorted[j] = sorted[j + 1U];
                sorted[j + 1U] = tmp;
            }
        }
    }

    return sorted[count / 2U];
}

void filter_init(filter_ch_t *f)
{
    memset(f, 0, sizeof(filter_ch_t));
}

uint16_t filter_update(filter_ch_t *f, uint16_t raw)
{
    uint8_t count;

    if (f == NULL) {
        return raw;
    }

    /* 限幅：12 位 ADC 合法范围 0~4095，防止异常值污染窗口 */
    if (raw > 4095U) {
        raw = 4095U;
    }

    /* 窗口已满时先减掉即将被覆盖的旧值 */
    if (f->filled != 0U) {
        f->sum -= f->window[f->index];
    }

    f->window[f->index] = raw;
    f->sum += raw;
    f->index++;

    if (f->index >= FILTER_WINDOW_SIZE) {
        f->index = 0U;
        f->filled = 1U;
    }

    count = (f->filled != 0U) ? FILTER_WINDOW_SIZE : f->index;
    if (count == 0U) {
        count = 1U;
    }

    f->avg = (uint16_t)(f->sum / count);

    if (count >= 4U) {
        uint16_t med = filter_median(f->window, count);
        uint32_t diff = (f->avg > med) ? (uint32_t)(f->avg - med) : (uint32_t)(med - f->avg);
        if ((float)diff > (float)f->avg * FILTER_MEDIAN_RATIO) {
            return med;
        }
    }

    return f->avg;
}

uint16_t filter_get_avg(const filter_ch_t *f)
{
    if (f == NULL) {
        return 0U;
    }
    return f->avg;
}
