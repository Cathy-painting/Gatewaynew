#ifndef TERMINAL_SERVICE_H
#define TERMINAL_SERVICE_H

#include <stdint.h>
#include "terminal_data.h"

void terminal_init(void);
void terminal_set_local_value(uint16_t value);
void terminal_set_remote_value(uint16_t value);
void terminal_set_remote_online(uint8_t online);
void terminal_inc_modbus_ok(void);
void terminal_inc_modbus_fail(void);
void terminal_inc_upload(void);
void terminal_set_cloud_online(uint8_t online);
void terminal_get_snapshot(terminal_data_t *data);

#endif
