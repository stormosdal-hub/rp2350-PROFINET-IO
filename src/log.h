/*
 * log.h - the application's log lines, "I (ms) tag: message", through the port's log buffer: they
 * never wait for USB (lib/pnet/port/log_rp2.c).
 */
#pragma once

#include "pnal_rp2.h"

#define LOGE(tag, ...) pnal_rp2_logf('E', tag, __VA_ARGS__)
#define LOGW(tag, ...) pnal_rp2_logf('W', tag, __VA_ARGS__)
#define LOGI(tag, ...) pnal_rp2_logf('I', tag, __VA_ARGS__)
