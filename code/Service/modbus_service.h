#ifndef MODBUS_SERVICE_H
#define MODBUS_SERVICE_H

#include <stdint.h>

/* 最大从站列表长度 */
#define MODBUS_MAX_SLAVES  4U

void modbus_service_init(void);
void modbus_service_poll_once(void);
uint8_t modbus_service_write_single(uint16_t reg_addr, uint16_t value);

#endif
