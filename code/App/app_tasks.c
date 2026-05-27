#include "headfile.h"
#include "bsp_adc.h"
#include "sample_service.h"

void app_init_before_scheduler(void)
{
    terminal_init();
    bsp_led_write8(0x00);
    bsp_rs485_set_rx_mode();
	
   // bsp_uart1_start_receive();
   // bsp_uart2_start_receive();
   // bsp_uart3_start_receive();
	  
	  bsp_uart2_rx_start();
		bsp_uart3_rx_start();

    log_info("[BOOT] system start\r\n");
}

void app_led_task(void)
{
    bsp_led_toggle(1);
    osDelay(500);
}

void app_sample_task(void)
{
	sample_service_process();
	osDelay(1000);
}

void app_modbus_task(void)
{
    osDelay(1000);
}

void app_log_task(void)
{
    terminal_data_t data;
    char buf[160];

    log_info("[LOG] logTask running\r\n");
    
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
