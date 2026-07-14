#ifndef BSP_RS485_H
#define BSP_RS485_H

#include "main.h"
#include <stdint.h>

void bsp_rs485_init(void);
void bsp_rs485_set_tx_mode(void);
void bsp_rs485_set_rx_mode(void);
void bsp_rs485_send(const uint8_t *data, uint16_t len);

#endif
