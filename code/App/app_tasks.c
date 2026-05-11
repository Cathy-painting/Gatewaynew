#include "headfile.h"

void app_init_before_scheduler(void)
{
    terminal_init();
    bsp_led_write8(0x00);
    bsp_rs485_set_rx_mode();
    bsp_uart3_rx_start();
    log_info("[BOOT] system start\r\n");
}

void app_led_task(void)
{
    bsp_led_toggle(0);
    osDelay(500);
}

void app_sample_task(void)
{
    uint16_t value;
    char buf[64];

    value = sample_service_read_local();
    terminal_set_local_value(value);

    snprintf(buf, sizeof(buf), "[SAMPLE] local=%u\r\n", value);
    log_info(buf);

    osDelay(1000);
}

void app_modbus_task(void)
{
    modbus_service_poll_once();
    osDelay(1000);
}

void app_log_task(void)
{
    terminal_data_t data;
    char buf[160];

    terminal_get_snapshot(&data);
    snprintf(buf, sizeof(buf),
             "[STATE] local=%u remote=%u online=%u sample=%lu mb_ok=%lu mb_fail=%lu upload=%lu\r\n",
             data.local_value,
             data.remote_value,
             data.remote_online,
             data.sample_count,
             data.modbus_ok_count,
             data.modbus_fail_count,
             data.upload_count);
    log_info(buf);

    osDelay(2000);
}

void app_cloud_task(void)
{
    cloud_service_publish_once();
    osDelay(5000);
}
