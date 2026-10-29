/*
 * eth.h - the RJ45 (WIZ850io), for PROFINET.
 */
#pragma once

#include <stdbool.h>

/* Start the W5500 and lwIP on it, and hand the interface to p-net's platform layer
 * (pnal_rp2_attach). Call it from a task. 0 on success. */
int eth_start(void);

/* Whether the cable is in and the link negotiated. */
bool eth_link_up(void);
