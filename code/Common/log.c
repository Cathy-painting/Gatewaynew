#include "log.h"
#include "bsp_uart.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void log_send_string(const char *msg)
{
    if (msg == NULL) {
        return;
    }

    bsp_uart1_send_string(msg);
}

void log_info(const char *msg)
{
    log_send_string(msg);
}

void log_infof(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    int len;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        log_send_string(buf);
    }
}

void log_hex(const char *prefix, const uint8_t *data, uint16_t len)
{
    char buf[8];

    if (prefix != NULL) {
        log_send_string(prefix);
    }

    if (data == NULL || len == 0U) {
        log_send_string("(empty)\r\n");
        return;
    }

    for (uint16_t i = 0; i < len; i++) {
        snprintf(buf, sizeof(buf), "%02X ", data[i]);
        log_send_string(buf);
    }

    log_send_string("\r\n");
}
