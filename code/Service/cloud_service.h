#ifndef CLOUD_SERVICE_H
#define CLOUD_SERVICE_H

#include <stdint.h>

/* ESP32 通过 UART JSON 协议通信，不再需要 WiFi 配置 */
/* WiFi 由 ESP32 自己管理 */

void cloud_service_init(void);
void cloud_service_publish_once(void);
uint8_t cloud_service_is_ready(void);

/* 解析 ESP32 发来的 LED 控制指令 */
void cloud_service_parse_command(const char *json_str);

#endif
