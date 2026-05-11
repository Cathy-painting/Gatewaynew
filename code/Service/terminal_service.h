#ifndef TERMINAL_SERVICE_H
#define TERMINAL_SERVICE_H

#include <stdint.h>
#include "terminal_data.h"

void terminal_init(void);
void terminal_set_local_value(uint16_t value);
void terminal_get_snapshot(terminal_data_t *data);

#endif
