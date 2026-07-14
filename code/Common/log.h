#ifndef LOG_H
#define LOG_H

#include <stdint.h>

void log_info(const char *msg);
void log_infof(const char *fmt, ...);
void log_hex(const char *prefix, const uint8_t *data, uint16_t len);

#endif
