#ifndef DHT11_SERVICE_H
#define DHT11_SERVICE_H

#include "bsp_dht11.h"

/* 初始化 DHT11 传感器 */
void dht11_service_init(void);

/* 读取一次 DHT11 数据，更新到 terminal_data */
void dht11_service_read(void);

#endif
