#ifndef CLOUD_SERVICE_H
#define CLOUD_SERVICE_H

#include <stdint.h>

#define CLOUD_WIFI_SSID       "mywifi"
#define CLOUD_WIFI_PASSWORD   "12345678"
#define CLOUD_MQTT_HOST       "broker.emqx.io"
#define CLOUD_MQTT_PORT       1883U
#define CLOUD_MQTT_TOPIC      "gateway/stm32/data"
#define CLOUD_MQTT_CLIENT_ID  "stm32_gateway_g431"

void cloud_service_init(void);
void cloud_service_publish_once(void);
uint8_t cloud_service_is_ready(void);

#endif
