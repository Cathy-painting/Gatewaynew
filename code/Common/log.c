#include "log.h"
#include "bsp_uart.h"
#include <stdio.h>

void log_info(const char *msg)
{
    bsp_uart1_send_string(msg);
}

void log_hex(const char *prefix, const uint8_t *data, uint16_t len)
{
    char buf[8];

    if (prefix != NULL) {
        log_info(prefix);
    }

    for (uint16_t i = 0; i < len; i++) {
        snprintf(buf, sizeof(buf), "%02X ", data[i]);
        log_info(buf);
    }

    log_info("\r\n");
}
