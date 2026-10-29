/*
 * app_log_rp2.c - the sample application's log (app_log.h) through the non-blocking log buffer
 * (pnal_rp2_log_start), in place of upstream app_log.c: its vprintf() is a blocking stdio write
 * in the task that sends the cyclic frames.
 */
#include <stdarg.h>
#include <stdio.h>
#include "app_log.h"
#include "pnal_rp2.h"

static int32_t log_level = APP_DEFAULT_LOG_LEVEL;

void app_log_set_log_level(int32_t level) { log_level = level; }

void app_log(int32_t level, const char *fmt, ...)
{
    if (level < log_level) return;
    va_list ap;
    va_start(ap, fmt);
    pnal_rp2_log_vprintf(fmt, ap);
    va_end(ap);
}

void app_log_print_bytes(int32_t level, const uint8_t *bytes, uint32_t len)
{
    char line[200];
    int n = snprintf(line, sizeof(line), "  Bytes: ");
    if (level < log_level) return;
    for (uint32_t i = 0; i < len && n < (int)sizeof(line) - 4; i++) n += snprintf(line + n, sizeof(line) - n, "%02X ", bytes[i]);
    line[n++] = '\n';
    pnal_rp2_log_write(line, n);
}
