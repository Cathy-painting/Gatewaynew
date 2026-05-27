#ifndef SAMPLE_SERVICE_H
#define SAMPLE_SERVICE_H

#include <stdint.h>

uint16_t sample_service_read_local(void);
void terminal_set_local_value(uint16_t value);
void sample_service_process(void);

#endif
