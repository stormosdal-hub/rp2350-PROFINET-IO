/*
 * pnal_config.h - the PNAL part of pnet_cfg_t on the RP2350: the stack's own background task.
 * Received frames come from the Ethernet driver's own task (eth.c), not from one p-net starts.
 */
#ifndef PNAL_CONFIG_H
#define PNAL_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pnal_thread_cfg {
    uint32_t prio;
    size_t stack_size;   // bytes
} pnal_thread_cfg_t;

typedef struct pnal_cfg {
    pnal_thread_cfg_t bg_worker_thread;   // saving to flash, port status
} pnal_cfg_t;

#ifdef __cplusplus
}
#endif

#endif /* PNAL_CONFIG_H */
