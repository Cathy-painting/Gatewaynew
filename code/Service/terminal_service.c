#include "terminal_service.h"
#include <string.h>

static terminal_data_t g_terminal_data;

void terminal_init(void)
{
    memset(&g_terminal_data, 0, sizeof(g_terminal_data));
}

void terminal_set_local_value(uint16_t value)
{
    g_terminal_data.local_value = value;
    g_terminal_data.sample_count++;
}

void terminal_get_snapshot(terminal_data_t *data)
{
    if (data == NULL) {
        return;
    }

    *data = g_terminal_data;
}
