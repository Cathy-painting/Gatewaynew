#include "terminal_service.h"
#include "main.h"
#include <string.h>

static terminal_data_t g_terminal_data;

static void terminal_lock(void)
{
    __disable_irq();
}

static void terminal_unlock(void)
{
    __enable_irq();
}

void terminal_init(void)
{
    terminal_lock();
    memset(&g_terminal_data, 0, sizeof(g_terminal_data));
    terminal_unlock();
}

void terminal_set_local_value(uint16_t value)
{
    terminal_lock();
    g_terminal_data.local_value = value;
    g_terminal_data.sample_count++;
    terminal_unlock();
}

void terminal_set_remote_value(uint16_t value)
{
    terminal_lock();
    g_terminal_data.remote_value = value;
    terminal_unlock();
}

void terminal_set_remote_online(uint8_t online)
{
    terminal_lock();
    g_terminal_data.remote_online = (online != 0U) ? 1U : 0U;
    terminal_unlock();
}

void terminal_inc_modbus_ok(void)
{
    terminal_lock();
    g_terminal_data.modbus_ok_count++;
    terminal_unlock();
}

void terminal_inc_modbus_fail(void)
{
    terminal_lock();
    g_terminal_data.modbus_fail_count++;
    terminal_unlock();
}

void terminal_inc_upload(void)
{
    terminal_lock();
    g_terminal_data.upload_count++;
    terminal_unlock();
}

void terminal_set_cloud_online(uint8_t online)
{
    terminal_lock();
    g_terminal_data.cloud_online = (online != 0U) ? 1U : 0U;
    terminal_unlock();
}

void terminal_get_snapshot(terminal_data_t *data)
{
    if (data == NULL) {
        return;
    }

    terminal_lock();
    *data = g_terminal_data;
    terminal_unlock();
}
