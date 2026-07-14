#ifndef MODBUS_SERVICE_H
#define MODBUS_SERVICE_H

#include <stdint.h>

void modbus_service_init(void);
void modbus_service_poll_once(void);
uint8_t modbus_service_write_single(uint16_t reg_addr, uint16_t value);

#endif
