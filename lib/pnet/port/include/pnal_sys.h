/*
 * pnal_sys.h - PNAL buffers on the RP2350: one heap block, the header with the frame right after
 * it (as on Linux). Received frames are read into it from the W5500, sent ones written out of it.
 */
#ifndef PNAL_SYS_H
#define PNAL_SYS_H

#include <stdint.h>
#include "osal.h"
#include "lwip/def.h"   // htonl, ntohs, ...

#ifdef __cplusplus
extern "C" {
#endif

#define PNAL_BUF_MAX_SIZE 1522   // largest Ethernet frame with one VLAN tag, without FCS

typedef struct pnal_buf {
    void *payload;
    uint16_t len;
} pnal_buf_t;

#ifdef __cplusplus
}
#endif

#endif /* PNAL_SYS_H */
