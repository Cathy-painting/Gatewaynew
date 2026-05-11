#ifndef APP_TASKS_H
#define APP_TASKS_H

void app_init_before_scheduler(void);
void app_led_task(void);
void app_sample_task(void);
void app_modbus_task(void);
void app_log_task(void);
void app_cloud_task(void);

#endif
