#include "event_log.h"
#include "terminal_service.h"
#include <string.h>

static event_entry_t g_events[EVENT_LOG_SIZE];
static uint8_t g_event_index = 0U;
static uint8_t g_event_count = 0U;

void event_log_init(void)
{
    memset(g_events, 0, sizeof(g_events));
    g_event_index = 0U;
    g_event_count = 0U;
}

void event_log_add(event_type_t type, uint8_t value)
{
    g_events[g_event_index].type      = type;
    g_events[g_event_index].value     = value;
    g_events[g_event_index].timestamp = terminal_get_uptime();
    g_events[g_event_index].reserved  = 0U;

    g_event_index++;
    if (g_event_index >= EVENT_LOG_SIZE) {
        g_event_index = 0U;
    }
    if (g_event_count < EVENT_LOG_SIZE) {
        g_event_count++;
    }
}

uint8_t event_log_get_recent(event_entry_t *out, uint8_t max_count)
{
    uint8_t count = 0U;
    if (out == NULL || g_event_count == 0U) return 0U;

    count = (g_event_count < max_count) ? g_event_count : max_count;

    /* 从最新事件往前遍历 */
    for (uint8_t i = 0U; i < count; i++) {
        int idx = (int)g_event_index - 1 - (int)i;
        while (idx < 0) idx += EVENT_LOG_SIZE;
        out[i] = g_events[(uint8_t)idx];
    }

    return count;
}

uint8_t event_log_count(void)
{
    return g_event_count;
}
