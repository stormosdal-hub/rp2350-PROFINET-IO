/*
 * eth.c - the RJ45 of the board: a WIZ850io (W5500) on SPI0, used as a plain MAC (MACRAW), so every
 * frame comes in here. PROFINET and LLDP frames go to p-net (pnal_rp2_input), ARP and IPv4 for
 * this device to lwIP, and everything else is dropped after reading only its header. No DHCP: a
 * PROFINET device starts at 0.0.0.0, or at the address from config.h, until TIA Portal or the PLC
 * gives it one by DCP (p-net then keeps it in flash).
 */
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "pico/time.h"
#include "pico/unique_id.h"
#include "lwip/etharp.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#include "config.h"
#include "log.h"
#include "pnal.h"
#include "pnal_rp2.h"
#include "w5500.h"
#include "eth.h"

static const char *TAG = "eth";

#define LINK_POLL_MS 250   // the PHY's link, speed and duplex
#define RX_POLL_MS   10    // the receive buffer, should INTn ever be missed

static const w5500_cfg_t chip_cfg = {
    .spi = SPI_INSTANCE(BOARD_ETH_SPI),
    .baud = PNIO_ETH_SPI_BAUD,
    .sck = BOARD_ETH_SCK_PIN,
    .mosi = BOARD_ETH_MOSI_PIN,
    .miso = BOARD_ETH_MISO_PIN,
    .cs = BOARD_ETH_CS_PIN,
    .rst = BOARD_ETH_RST_PIN,
};

static struct netif netif;
static SemaphoreHandle_t chip;   // the SPI bus and the W5500: one task at a time
static TaskHandle_t rx_task;
static pnal_rp2_link_t link;     // as last read from the PHY
static uint8_t mac[6];

static void lock(void) { xSemaphoreTake(chip, portMAX_DELAY); }
static void unlock(void) { xSemaphoreGive(chip); }

// "02:12:34:56:78:9a"
static bool parse_mac(const char *s, uint8_t out[6])
{
    for (int i = 0; i < 6; i++) {
        char *end;
        unsigned long v = strtoul(s, &end, 16);
        if (end == s || v > 0xff || (i < 5 ? *end != ':' : *end != '\0')) return false;
        out[i] = (uint8_t)v;
        s = end + 1;
    }
    return true;
}

// config.h's, or one made from the RP2350's unique ID: locally administered, unicast
static void make_mac(uint8_t out[6])
{
    if (strlen(PNIO_MAC)) {
        if (parse_mac(PNIO_MAC, out) && !(out[0] & 0x01)) return;
        LOGE(TAG, "PNIO_MAC \"%s\" is not a unicast MAC address: using the one from the unique ID", PNIO_MAC);
    }
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    uint64_t h = 0xcbf29ce484222325ull;   // FNV-1a
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        h ^= id.id[i];
        h *= 0x100000001b3ull;
    }
    out[0] = 0x02;
    for (int i = 1; i < 6; i++) out[i] = (uint8_t)(h >> (8 * (i - 1)));
}

// one frame out, from a pbuf chain (lwIP) or a buffer (p-net)
static bool transmit(const struct pbuf *p, const uint8_t *frame, size_t len)
{
    bool ok = false, reopened = false;
    lock();
    if (w5500_tx_begin(len)) {
        if (p) {
            for (const struct pbuf *q = p; q; q = q->next) w5500_tx_append(q->payload, q->len);
        } else {
            w5500_tx_append(frame, len);
        }
        ok = w5500_tx_commit();
        if (!ok) reopened = w5500_reopen();   // the chip did not confirm: start its socket over
    }
    unlock();
    if (!ok && reopened) LOGW(TAG, "a frame was not confirmed sent: socket reopened");
    return ok;
}

// lwIP's frames; with the link down they are dropped, as a cable would drop them
static err_t linkoutput(struct netif *n, struct pbuf *p)
{
    if (!link.up) return ERR_OK;
    return transmit(p, NULL, p->tot_len) ? ERR_OK : ERR_IF;
}

// p-net's frames (pnal_eth_send)
static int send_frame(const uint8_t *frame, size_t len) { return transmit(NULL, frame, len) ? 0 : -1; }

static void get_link(pnal_rp2_link_t *out)
{
    taskENTER_CRITICAL();
    *out = link;
    taskEXIT_CRITICAL();
}

static const pnal_rp2_eth_driver_t driver = { .send = send_frame, .get_link = get_link };

bool eth_link_up(void) { return link.up; }

static err_t init_netif(struct netif *n)
{
    n->name[0] = 'e';
    n->name[1] = 'n';
    n->hwaddr_len = ETH_HWADDR_LEN;
    memcpy(n->hwaddr, mac, sizeof(mac));
    n->mtu = 1500;
    n->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;
    n->output = etharp_output;
    n->linkoutput = linkoutput;
    return ERR_OK;
}

// the address as DCP sets it: TIA Portal's "Assign IP address", or the PLC when it connects
static void on_status(struct netif *n)
{
    char ip[IP4ADDR_STRLEN_MAX], mask[IP4ADDR_STRLEN_MAX], gw[IP4ADDR_STRLEN_MAX];
    ip4addr_ntoa_r(netif_ip4_addr(n), ip, sizeof(ip));
    ip4addr_ntoa_r(netif_ip4_netmask(n), mask, sizeof(mask));
    ip4addr_ntoa_r(netif_ip4_gw(n), gw, sizeof(gw));
    LOGI(TAG, "IP address %s / %s, gateway %s", ip, mask, gw);
}

// config.h's address, if one is given
static void default_address(ip4_addr_t *ip, ip4_addr_t *mask, ip4_addr_t *gw)
{
    ip4_addr_set_zero(ip);
    ip4_addr_set_zero(mask);
    ip4_addr_set_zero(gw);
    if (!strlen(PNIO_IP)) return;
    if (!ip4addr_aton(PNIO_IP, ip) || !ip4addr_aton(PNIO_NETMASK, mask)) {
        LOGE(TAG, "bad default address %s / %s: starting at 0.0.0.0", PNIO_IP, PNIO_NETMASK);
        ip4_addr_set_zero(ip);
        ip4_addr_set_zero(mask);
        return;
    }
    if (strlen(PNIO_GATEWAY) && !ip4addr_aton(PNIO_GATEWAY, gw)) ip4_addr_set_zero(gw);
}

enum { DROP, TO_PNET, TO_LWIP };

static int classify(const uint8_t *f, size_t n)
{
    static const uint8_t broadcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    bool to_us = memcmp(f, mac, 6) == 0;
    bool multicast = f[0] & 0x01;   // broadcast too: DCP Identify, LLDP
    if (pnal_rp2_is_pnet_frame(f, n)) return to_us || multicast ? TO_PNET : DROP;
    uint16_t type = (uint16_t)(f[12] << 8 | f[13]);
    if (type == ETHTYPE_ARP && (to_us || memcmp(f, broadcast, 6) == 0)) return TO_LWIP;
    if (type == ETHTYPE_IP && to_us) return TO_LWIP;
    return DROP;
}

// the next frame in the W5500's buffer, read into a p-net buffer or a pbuf; false when it is empty
static bool receive_one(void)
{
    uint8_t head[PNAL_RP2_CLASSIFY_LEN];
    pnal_buf_t *frame = NULL;
    struct pbuf *packet = NULL;
    bool reopened = false;

    lock();
    int len = w5500_rx_peek(head, sizeof(head));
    if (len <= 0) {
        if (len < 0) reopened = w5500_reopen();
        unlock();
        if (len < 0) LOGW(TAG, "receive buffer out of step: socket %s", reopened ? "reopened" : "did not reopen");
        return false;
    }
    int kind = classify(head, (size_t)len < sizeof(head) ? (size_t)len : sizeof(head));
    if (kind == TO_PNET && (frame = pnal_buf_alloc((uint16_t)len)) != NULL) {
        w5500_rx_read(0, frame->payload, (size_t)len);
    } else if (kind == TO_LWIP && (packet = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL)) != NULL) {
        size_t offset = 0;
        for (struct pbuf *q = packet; q; q = q->next) {
            w5500_rx_read(offset, q->payload, q->len);
            offset += q->len;
        }
    }
    w5500_rx_done();
    unlock();

    pnal_rp2_count_in((size_t)len, kind != DROP && !frame && !packet);   // wanted, but out of memory
    if (frame) pnal_rp2_input(frame);
    if (packet && netif.input(packet, &netif) != ERR_OK) pbuf_free(packet);
    return true;
}

static void check_link(void)
{
    w5500_phy_t phy;
    bool socket_ok, restarted = false;
    lock();
    socket_ok = w5500_socket_ok();
    if (!socket_ok) restarted = w5500_init(&chip_cfg, mac) >= 0;   // reset under us (brown-out?): set it up again
    w5500_get_phy(&phy);
    unlock();
    if (!socket_ok) LOGW(TAG, "the W5500 lost its settings: %s", restarted ? "set up again" : "no answer");

    pnal_rp2_link_t now = { .up = phy.link, .speed_100 = phy.speed_100, .full_duplex = phy.full_duplex };
    bool changed = now.up != link.up;
    taskENTER_CRITICAL();
    link = now;
    taskEXIT_CRITICAL();
    if (!changed) return;

    if (now.up) {
        LOGI(TAG, "link up, %s %s duplex, MAC %02x:%02x:%02x:%02x:%02x:%02x", now.speed_100 ? "100 Mbit/s" : "10 Mbit/s",
             now.full_duplex ? "full" : "half", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        LOGW(TAG, "link down (cable unplugged?)");
    }
    LOCK_TCPIP_CORE();
    if (now.up) netif_set_link_up(&netif);
    else netif_set_link_down(&netif);
    UNLOCK_TCPIP_CORE();
}

// INTn: the W5500 holds it low while "socket 0 received" is set
static void on_int(void)
{
    if (!(gpio_get_irq_event_mask(BOARD_ETH_INT_PIN) & GPIO_IRQ_EDGE_FALL)) return;
    gpio_acknowledge_irq(BOARD_ETH_INT_PIN, GPIO_IRQ_EDGE_FALL);
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(rx_task, &woken);
    portYIELD_FROM_ISR(woken);
}

static void rx_task_fn(void *arg)
{
    absolute_time_t next_link_check = get_absolute_time();
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(RX_POLL_MS));
        if (time_reached(next_link_check)) {
            check_link();
            next_link_check = make_timeout_time_ms(LINK_POLL_MS);
        }
        // clear the interrupt, then empty the buffer; INTn low again: a frame came in meanwhile
        for (int round = 0; round < 4; round++) {
            lock();
            w5500_clear_rx_interrupt();
            unlock();
            while (receive_one()) {
            }
            if (gpio_get(BOARD_ETH_INT_PIN)) break;
        }
    }
}

int eth_start(void)
{
    chip = xSemaphoreCreateMutex();
    if (!chip) return -1;
    make_mac(mac);
    lock();
    int version = w5500_init(&chip_cfg, mac);
    unlock();
    if (version < 0) {
        LOGE(TAG, "the W5500 (WIZ850io) does not answer: check SPI%d (SCK %d, MOSI %d, MISO %d, CS %d) and RST %d",
             BOARD_ETH_SPI, BOARD_ETH_SCK_PIN, BOARD_ETH_MOSI_PIN, BOARD_ETH_MISO_PIN, BOARD_ETH_CS_PIN,
             BOARD_ETH_RST_PIN);
        return -1;
    }
    LOGI(TAG, "W5500 version %d, SPI at %lu Hz, MAC %02x:%02x:%02x:%02x:%02x:%02x", version,
         (unsigned long)spi_get_baudrate(chip_cfg.spi), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ip4_addr_t ip, mask, gw;
    default_address(&ip, &mask, &gw);
    tcpip_init(NULL, NULL);
    LOCK_TCPIP_CORE();
    struct netif *n = netif_add(&netif, &ip, &mask, &gw, NULL, init_netif, tcpip_input);
    if (n) {
        netif_set_status_callback(&netif, on_status);
        netif_set_default(&netif);
        netif_set_up(&netif);
    }
    UNLOCK_TCPIP_CORE();
    if (!n || pnal_rp2_attach(&netif, &driver) != 0) {
        LOGE(TAG, "lwIP interface could not be added");
        return -1;
    }

    if (xTaskCreate(rx_task_fn, "pn_eth", PNIO_RX_TASK_STACK / sizeof(StackType_t), NULL, PNIO_RX_TASK_PRIO,
                    &rx_task) != pdPASS) {
        LOGE(TAG, "receive task could not be created");
        return -1;
    }
    gpio_init(BOARD_ETH_INT_PIN);
    gpio_set_dir(BOARD_ETH_INT_PIN, GPIO_IN);
    gpio_pull_up(BOARD_ETH_INT_PIN);
    gpio_add_raw_irq_handler(BOARD_ETH_INT_PIN, on_int);
    gpio_set_irq_enabled(BOARD_ETH_INT_PIN, GPIO_IRQ_EDGE_FALL, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
    return 0;
}
