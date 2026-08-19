#ifndef BSP_RS485_H
#define BSP_RS485_H

#include "main.h"
#include <stdint.h>

/* RS485 收发切换延时（us），适配 115200bps：
   1 字符 = 11 bits / 115200 ≈ 95.5us
   50us 足够覆盖 DE 引脚建立时间和一次字符间隔 */
#define RS485_TURNAROUND_US  50U

/* RS485 发送超时（ms） */
#define RS485_TX_TIMEOUT_MS  500U

void bsp_rs485_init(void);
void bsp_rs485_set_tx_mode(void);
void bsp_rs485_set_rx_mode(void);
void bsp_rs485_send(const uint8_t *data, uint16_t len);

#endif
