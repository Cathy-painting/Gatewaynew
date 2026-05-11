#include "crc16.h"

uint16_t crc16_modbus(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    if (data == 0) {
        return 0;
    }

    for (uint16_t pos = 0; pos < len; pos++) {
        crc ^= data[pos];

        for (uint8_t i = 0; i < 8; i++) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}
