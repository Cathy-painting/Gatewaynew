#ifndef BSP_DHT11_H
#define BSP_DHT11_H

#include <stdint.h>

/* ========================================================================
   DHT11 温湿度传感器驱动（单总线协议）

   接线：PA1 (J3排针) ←→ DHT11 DATA + 4.7kΩ~10kΩ 上拉电阻到 3.3V
                               DHT11 VCC  → 3.3V
                               DHT11 GND  → GND

   协议：MCU 发启动信号（低 18ms+高 30us）→ DHT11 应答（低 80us+高 80us）
        → 40bit 数据（湿度高8+湿度低8+温度高8+温度低8+校验8）
   ======================================================================== */

#define DHT11_GPIO_PORT  GPIOA
#define DHT11_GPIO_PIN   GPIO_PIN_1

/* DHT11 数据结构 */
typedef struct {
    uint8_t  humidity;    /* 湿度 %RH（整数部分） */
    uint8_t  humidity_dec; /* 湿度小数（通常为 0） */
    uint8_t  temperature;  /* 温度 ℃（整数部分） */
    uint8_t  temp_dec;     /* 温度小数（通常为 0） */
    uint8_t  checksum;     /* 校验和 */
    uint8_t  valid;        /* 数据是否有效 */
} dht11_data_t;

/* 初始化 DHT11 GPIO */
void bsp_dht11_init(void);

/* 读取一次数据，返回 1 表示成功 */
uint8_t bsp_dht11_read(dht11_data_t *data);

#endif
