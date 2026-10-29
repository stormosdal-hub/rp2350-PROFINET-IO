/*
 * pnal_eth_rp2.c - raw Ethernet for p-net on the RP2350. The Ethernet driver's receive task reads
 * every frame; PROFINET (0x8892) and LLDP (0x88CC) frames, also VLAN-tagged as controllers send
 * them (priority 6), come here and go to p-net in that task. Frames go out through the driver,
 * which serialises them with lwIP's.
 */
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "pnal.h"
#include "osal_log.h"
#include "pnal_rp2.h"
#include "pnal_rp2_priv.h"

struct pnal_eth_handle {
    pnal_eth_callback_t *callback;
    void *arg;
};

struct netif *pnal_rp2_netif;
static const pnal_rp2_eth_driver_t *driver;
static pnal_eth_handle_t *pending;             // set by pnal_eth_init(), inside pnet_init()
static pnal_eth_handle_t *volatile receiver;   // = pending once pnet_init() has returned
static pnal_port_stats_t stats;                // all frames in, p-net's frames out

// pnio and the receive task both send: count atomically
#define COUNT(field, n) __atomic_fetch_add(&stats.field, (uint32_t)(n), __ATOMIC_RELAXED)

// PROFINET or LLDP behind at most two VLAN tags, with room for the frame ID after the type
bool pnal_rp2_is_pnet_frame(const uint8_t *frame, size_t len)
{
    size_t pos = 12;
    for (int tags = 0;; tags++) {
        if (len < pos + 4) return false;
        uint16_t type = (uint16_t)(frame[pos] << 8 | frame[pos + 1]);
        if (type == PNAL_ETHTYPE_PROFINET || type == PNAL_ETHTYPE_LLDP) return true;
        if (type != PNAL_ETHTYPE_VLAN || tags == 2) return false;
        pos += 4;
    }
}

void pnal_rp2_count_in(size_t len, bool discarded)
{
    COUNT(if_in_octets, len);
    if (discarded) COUNT(if_in_discards, 1);
}

void pnal_rp2_input(pnal_buf_t *p)
{
    pnal_eth_handle_t *r = receiver;
    if (!r) {   // p-net not started yet (pnal_rp2_start_input)
        COUNT(if_in_discards, 1);
        pnal_buf_free(p);
        return;
    }
    if (r->callback(r, r->arg, p) == 0) pnal_buf_free(p);   // 1: p-net handled and freed it
}

int pnal_rp2_attach(struct netif *netif, const pnal_rp2_eth_driver_t *drv)
{
    if (!netif || !drv || !drv->send || !drv->get_link) return -1;
    pnal_rp2_netif = netif;
    driver = drv;
    pnal_rp2_files_init();   // without it p-net still runs, with its defaults (and says so)
    return 0;
}

// one interface (PNET_MAX_PHYSICAL_PORTS 1): p-net asks for all frame types on it
pnal_eth_handle_t *pnal_eth_init(const char *if_name, pnal_ethertype_t receive_type, const pnal_cfg_t *pnal_cfg,
                                 pnal_eth_callback_t *callback, void *arg)
{
    if (!driver) {
        os_log(LOG_LEVEL_ERROR, "pnal: \"%s\": call pnal_rp2_attach() before pnet_init()\n", if_name);
        return NULL;
    }
    if (pending) {
        os_log(LOG_LEVEL_ERROR, "pnal: \"%s\": one Ethernet interface only\n", if_name);
        return NULL;
    }
    pnal_eth_handle_t *h = pvPortMalloc(sizeof(*h));
    if (!h) return NULL;
    h->callback = callback;
    h->arg = arg;
    pending = h;
    return h;
}

// The receive task runs above the task in pnet_init(): a frame handed over before p-net's
// scheduler and LLDP mutex exist would reach them half made
void pnal_rp2_start_input(void) { receiver = pending; }

// with the link down a frame is dropped as a cable would drop it, not reported as an error
int pnal_eth_send(pnal_eth_handle_t *handle, pnal_buf_t *buf)
{
    pnal_rp2_link_t link;
    if (!buf || !driver) return -1;
    driver->get_link(&link);
    if (!link.up) {
        COUNT(if_out_discards, 1);
        return buf->len;
    }
    if (driver->send(buf->payload, buf->len) != 0) {
        COUNT(if_out_errors, 1);
        return -1;
    }
    COUNT(if_out_octets, buf->len);
    return buf->len;
}

int pnal_eth_get_status(const char *interface_name, pnal_eth_status_t *status)
{
    pnal_rp2_link_t link;
    if (!driver) return -1;
    driver->get_link(&link);

    status->is_autonegotiation_supported = true;
    status->is_autonegotiation_enabled = true;   // the WIZ850io's PMODE pins: all capable, auto-negotiation
    status->autonegotiation_advertised_capabilities =
        PNAL_ETH_AUTONEG_CAP_100BaseTX_FULL_DUPLEX | PNAL_ETH_AUTONEG_CAP_100BaseTX_HALF_DUPLEX |
        PNAL_ETH_AUTONEG_CAP_10BaseT_FULL_DUPLEX | PNAL_ETH_AUTONEG_CAP_10BaseT_HALF_DUPLEX;
    status->running = link.up;
    if (!link.up) status->operational_mau_type = PNAL_ETH_MAU_UNKNOWN;
    else if (link.speed_100)
        status->operational_mau_type = link.full_duplex ? PNAL_ETH_MAU_COPPER_100BaseTX_FULL_DUPLEX
                                                        : PNAL_ETH_MAU_COPPER_100BaseTX_HALF_DUPLEX;
    else status->operational_mau_type = PNAL_ETH_MAU_COPPER_10BaseT;
    return 0;
}

int pnal_get_port_statistics(const char *interface_name, pnal_port_stats_t *port_stats)
{
    *port_stats = stats;
    return 0;
}

int pnal_get_interface_index(const char *interface_name) { return 1; }

int pnal_get_macaddress(const char *interface_name, pnal_ethaddr_t *p_mac)
{
    if (!pnal_rp2_netif) return -1;
    memcpy(p_mac->addr, pnal_rp2_netif->hwaddr, sizeof(p_mac->addr));
    return 0;
}
