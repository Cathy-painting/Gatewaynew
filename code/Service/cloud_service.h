#ifndef CLOUD_SERVICE_H
#define CLOUD_SERVICE_H

#include <stdint.h>

/* ====== WiFi 配置 ====== */
#define CLOUD_WIFI_SSID       "111"
#define CLOUD_WIFI_PASSWORD   "amg1408700"

/* ====== ESP8266 HTTP Server 配置 ====== */
/* ESP8266 作为 HTTP 服务器，手机浏览器直接访问 ESP8266 的 IP */
#define CLOUD_SERVER_PORT     8080U

void cloud_service_init(void);
void cloud_service_publish_once(void);
uint8_t cloud_service_is_ready(void);

#endif /* CLOUD_SERVICE_H */
