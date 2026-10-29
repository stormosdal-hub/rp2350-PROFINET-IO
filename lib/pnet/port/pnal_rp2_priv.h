/*
 * pnal_rp2_priv.h - the interface p-net runs on, shared by the port's files (set by
 * pnal_rp2_attach()).
 */
#pragma once

#include "lwip/netif.h"

extern struct netif *pnal_rp2_netif;

/* The flash area for p-net's files (files_rp2.c); pnal_rp2_attach() sets it up. */
int pnal_rp2_files_init(void);
