#include "headfile.h"
#include "bsp_adc.h"
#include "sample_service.h"

void app_init_before_scheduler(void)
{
    terminal_init();
    modbus_service_init();
    cloud_service_init();
    bsp_led_write8(0x00);
    bsp_rs485_init();
    bsp_rs485_set_rx_mode();

    bsp_lcd_init();

    bsp_uart1_rx_start();
    bsp_uart3_rx_start();

    log_info("[BOOT] system start\r\n");
}

void app_led_task(void)
{
    bsp_led_toggle(1);
    bsp_lcd_show_test_info();
    osDelay(500);
}

void app_sample_task(void)
{
	sample_service_process();
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

    log_info("[LOG] logTask running\r\n");
    
    terminal_get_snapshot(&data);
    snprintf(buf, sizeof(buf),
             "[STATE] local=%u remote=%u mb_online=%u cloud=%u sample=%lu mb_ok=%lu mb_fail=%lu upload=%lu\r\n",
             data.local_value,
             data.remote_value,
             data.remote_online,
             data.cloud_online,
             data.sample_count,
             data.modbus_ok_count,
             data.modbus_fail_count,
             data.upload_count);
    log_info(buf);

    osDelay(2000);
}

void app_cloud_task(void)
{
    log_info("[CLOUD] app_cloud_task running\r\n");
    cloud_service_publish_once();
    osDelay(5000);
}
