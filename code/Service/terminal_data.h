#ifndef TERMINAL_DATA_H
#define TERMINAL_DATA_H

#include <stdint.h>

typedef struct {
    uint16_t local_value;
    uint16_t remote_value;
    uint8_t remote_online;
    uint32_t sample_count;
    uint32_t modbus_ok_count;
    uint32_t modbus_fail_count;
    uint32_t upload_count;
} terminal_data_t;

#endif
