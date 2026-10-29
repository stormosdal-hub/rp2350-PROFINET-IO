/*
 * pnal_rp2.h - giving the Ethernet interface to the PROFINET stack, and the log that never blocks.
 */
#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lwip/netif.h"
#include "pnal_sys.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The name p-net knows the interface by (pnet_cfg_t if_cfg). */
#define PNAL_RP2_NETIF_NAME "eth0"

typedef struct pnal_rp2_link {
    bool up;
    bool speed_100;     // false: 10 Mbit/s
    bool full_duplex;
} pnal_rp2_link_t;

/* What the port needs from the Ethernet driver, besides lwIP's netif. */
typedef struct pnal_rp2_eth_driver {
    /* Send one frame (no FCS), serialised with lwIP's frames. 0 when it went out. */
    int (*send)(const uint8_t *frame, size_t len);
    /* The PHY's link, speed and duplex. */
    void (*get_link)(pnal_rp2_link_t *link);
} pnal_rp2_eth_driver_t;

/*
 * The interface p-net runs on: lwIP's netif for the MAC and the IP suite, the driver for raw frames
 * and the link. Call it before pnet_init(). From then on the driver hands PROFINET (0x8892) and
 * LLDP (0x88CC) frames to pnal_rp2_input(), which passes them on after pnal_rp2_start_input().
 */
int pnal_rp2_attach(struct netif *netif, const pnal_rp2_eth_driver_t *driver);

/* What pnal_rp2_is_pnet_frame() needs of a frame: up to two VLAN tags and the type after them. */
#define PNAL_RP2_CLASSIFY_LEN 24

/* Start handing received frames to p-net: call it once pnet_init() (app_init()) has returned.
 * Until then they are dropped, and counted as discards. */
void pnal_rp2_start_input(void);

/* A frame for p-net: PROFINET or LLDP, also behind VLAN tags as controllers send them. */
bool pnal_rp2_is_pnet_frame(const uint8_t *frame, size_t len);

/*
 * A received frame for p-net, in the driver's receive task. The buffer (pnal_buf_alloc) is the
 * stack's from here: freed when p-net has not started yet or does not want it.
 */
void pnal_rp2_input(pnal_buf_t *frame);

/* Every frame received (lwIP's and dropped ones too), for the port statistics. */
void pnal_rp2_count_in(size_t len, bool discarded);

/*
 * Logging that never blocks (log_rp2.c): from here on p-net's log and whatever uses the functions
 * below go through a ring buffer to a low-priority task that prints to stdio (USB). Call it first
 * in main(). A blocking write in the task that sends the cyclic frames would make the PLC's
 * watchdog expire.
 */
int pnal_rp2_log_start(void);

/* vprintf() for the log: formats one line and queues it, dropping it if the buffer is full. */
int pnal_rp2_log_vprintf(const char *fmt, va_list ap);

/* Queue text that is already formatted. */
void pnal_rp2_log_write(const char *line, size_t len);

/* One line "L (ms) tag: message\n", as ESP-IDF's log prints them. */
void pnal_rp2_logf(char level, const char *tag, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

#ifdef __cplusplus
}
#endif
