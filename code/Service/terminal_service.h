#ifndef TERMINAL_SERVICE_H
#define TERMINAL_SERVICE_H

#include <stdint.h>
#include "terminal_data.h"

void terminal_init(void);

/* ADC 采样数据更新 */
void terminal_update_sample(uint16_t ch, uint16_t raw, uint16_t filtered,
                            float voltage, float temperature);
void terminal_update_alarms(void);

/* Modbus 远程数据 */
void terminal_set_remote_value(uint16_t value);
void terminal_set_remote_value2(uint16_t value);
void terminal_set_remote_online(uint8_t online);
void terminal_inc_modbus_ok(void);
void terminal_inc_modbus_fail(void);

/* 云端 */
void terminal_set_cloud_online(uint8_t online);
void terminal_inc_upload(void);

/* 快照：原子读取全部状态 */
void terminal_get_snapshot(terminal_data_t *data);

/* DHT11 温湿度 */
void terminal_set_dht11(uint8_t humidity, float temperature);
void terminal_set_dht11_invalid(void);

/* 兼容旧代码 */
void terminal_set_local_value(uint16_t value);

/* 递增采样计数（由 sample_service 调用） */
void terminal_inc_sample_count(void);

/* 告警状态变化通知回调（由 app_tasks 注册，用于 LED 联动） */
typedef void (*terminal_alarm_cb_t)(uint8_t old_status, uint8_t new_status);
void terminal_set_alarm_callback(terminal_alarm_cb_t cb);

/* 系统运行时间 */
uint32_t terminal_get_uptime(void);
void terminal_inc_uptime(void);

#endif
