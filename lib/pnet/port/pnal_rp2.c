/*
 * pnal_rp2.c - the rest of p-net's platform layer on the RP2350: the IP suite (lwIP's netif, never
 * DHCP: a PROFINET device gets its address by DCP), buffers and uptime. The stack's "files" are in
 * files_rp2.c.
 */
#include <stdio.h>
#include <string.h>
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "pico/time.h"
#include "osal_log.h"
#include "pnal.h"
#include "pnal_rp2_priv.h"

static char hostname[PNAL_HOSTNAME_MAX_SIZE];   // the station name, as p-net last set it

int pnal_set_ip_suite(const char *interface_name, const pnal_ipaddr_t *p_ipaddr, const pnal_ipaddr_t *p_netmask,
                      const pnal_ipaddr_t *p_gw, const char *name, bool permanent)
{
    ip4_addr_t ip, netmask, gw;
    if (!pnal_rp2_netif) return -1;
    ip4_addr_set_u32(&ip, htonl(*p_ipaddr));
    ip4_addr_set_u32(&netmask, htonl(*p_netmask));
    ip4_addr_set_u32(&gw, htonl(*p_gw));
    LOCK_TCPIP_CORE();
    netif_set_addr(pnal_rp2_netif, &ip, &netmask, &gw);   // logged by eth.c's status callback
    UNLOCK_TCPIP_CORE();
    if (name) snprintf(hostname, sizeof(hostname), "%s", name);
    return 0;
}

typedef struct {
    uint32_t ip, netmask, gw;   // network byte order
} ip_info_t;

static ip_info_t ip_info(void)
{
    ip_info_t info = { 0 };
    if (!pnal_rp2_netif) return info;
    LOCK_TCPIP_CORE();
    info.ip = ip4_addr_get_u32(netif_ip4_addr(pnal_rp2_netif));
    info.netmask = ip4_addr_get_u32(netif_ip4_netmask(pnal_rp2_netif));
    info.gw = ip4_addr_get_u32(netif_ip4_gw(pnal_rp2_netif));
    UNLOCK_TCPIP_CORE();
    return info;
}

pnal_ipaddr_t pnal_get_ip_address(const char *interface_name) { return ntohl(ip_info().ip); }
pnal_ipaddr_t pnal_get_netmask(const char *interface_name) { return ntohl(ip_info().netmask); }
pnal_ipaddr_t pnal_get_gateway(const char *interface_name) { return ntohl(ip_info().gw); }

int pnal_get_hostname(char *out)
{
    snprintf(out, PNAL_HOSTNAME_MAX_SIZE, "%s", hostname);
    return 0;
}

int pnal_get_ip_suite(const char *interface_name, pnal_ipaddr_t *p_ipaddr, pnal_ipaddr_t *p_netmask,
                      pnal_ipaddr_t *p_gw, char *name)
{
    ip_info_t info = ip_info();
    *p_ipaddr = ntohl(info.ip);
    *p_netmask = ntohl(info.netmask);
    *p_gw = ntohl(info.gw);
    return pnal_get_hostname(name);
}

uint32_t pnal_get_system_uptime_10ms(void) { return (uint32_t)(time_us_64() / 10000); }

// the frame right after the header, in one block
pnal_buf_t *pnal_buf_alloc(uint16_t length)
{
    pnal_buf_t *p = os_malloc(sizeof(*p) + length);
    if (!p) return NULL;
    p->payload = p + 1;
    p->len = length;
    return p;
}

void pnal_buf_free(pnal_buf_t *p) { os_free(p); }

uint8_t pnal_buf_header(pnal_buf_t *p, int16_t header_size_increment) { return 255; }   // unused by p-net
