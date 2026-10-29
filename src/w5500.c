/*
 * w5500.c - the W5500's registers over SPI (mode 0, variable length data frames: 16-bit address,
 * control byte, data) and its socket 0 as a MACRAW socket. Registers and sequences from the W5500
 * datasheet (v1.1) and WIZnet's ioLibrary. The chip wraps addresses inside a socket's buffer
 * itself, so buffer pointers are plain 16-bit offsets.
 */
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include "w5500.h"

// control byte: block (BSB) << 3 | write << 2 | variable length data mode (00)
#define BLOCK_COMMON        0x00
#define BLOCK_SOCKET(n)     ((uint8_t)((n) * 4 + 1))
#define BLOCK_S0            BLOCK_SOCKET(0)
#define BLOCK_S0_TX         0x02
#define BLOCK_S0_RX         0x03
#define WRITE               0x04

// common registers
#define MR       0x0000
#define SHAR     0x0009
#define INTLEVEL 0x0013
#define IMR      0x0016
#define SIMR     0x0018
#define PHYCFGR  0x002e
#define VERSIONR 0x0039

// socket registers
#define Sn_MR         0x0000
#define Sn_CR         0x0001
#define Sn_IR         0x0002
#define Sn_SR         0x0003
#define Sn_RXBUF_SIZE 0x001e
#define Sn_TXBUF_SIZE 0x001f
#define Sn_TX_FSR     0x0020
#define Sn_TX_WR      0x0024
#define Sn_RX_RSR     0x0026
#define Sn_RX_RD      0x0028
#define Sn_IMR        0x002c

#define MR_RST       0x80
#define Sn_MR_MACRAW 0x04
#define Sn_MR_MIP6B  0x10   // MACRAW: drop IPv6 (MFEN off: every other frame comes in, multicast too)
#define Sn_CR_OPEN   0x01
#define Sn_CR_CLOSE  0x10
#define Sn_CR_SEND   0x20
#define Sn_CR_RECV   0x40
#define Sn_IR_SENDOK 0x10
#define Sn_IR_RECV   0x04
#define SOCK_CLOSED  0x00
#define SOCK_MACRAW  0x42
#define PHY_LNK      0x01
#define PHY_SPD      0x02
#define PHY_DPX      0x04
#define VERSION      0x04

#define MIN_FRAME 60     // without FCS: shorter ones are padded
#define MAX_FRAME 1522   // with a VLAN tag, without FCS

static w5500_cfg_t cfg;
static uint16_t rx_rd, rx_len;   // the frame w5500_rx_peek() found
static uint16_t tx_wr;           // where the frame being sent starts
static size_t tx_len;
static bool tx_open;

static void frame_start(uint16_t addr, uint8_t control)
{
    const uint8_t header[3] = { (uint8_t)(addr >> 8), (uint8_t)addr, control };
    gpio_put(cfg.cs, 0);
    spi_write_blocking(cfg.spi, header, sizeof(header));
}

static void read_buf(uint8_t block, uint16_t addr, void *dst, size_t len)
{
    frame_start(addr, (uint8_t)(block << 3));
    spi_read_blocking(cfg.spi, 0, dst, len);
    gpio_put(cfg.cs, 1);
}

static void write_buf(uint8_t block, uint16_t addr, const void *src, size_t len)
{
    frame_start(addr, (uint8_t)(block << 3 | WRITE));
    spi_write_blocking(cfg.spi, src, len);
    gpio_put(cfg.cs, 1);
}

static uint8_t read8(uint8_t block, uint16_t addr)
{
    uint8_t v;
    read_buf(block, addr, &v, 1);
    return v;
}

static void write8(uint8_t block, uint16_t addr, uint8_t v) { write_buf(block, addr, &v, 1); }

static uint16_t read16(uint8_t block, uint16_t addr)
{
    uint8_t b[2];
    read_buf(block, addr, b, 2);
    return (uint16_t)(b[0] << 8 | b[1]);
}

static void write16(uint8_t block, uint16_t addr, uint16_t v)
{
    const uint8_t b[2] = { (uint8_t)(v >> 8), (uint8_t)v };
    write_buf(block, addr, b, 2);
}

// the free and received sizes change while they are read: read until two reads agree
static uint16_t read16_stable(uint8_t block, uint16_t addr)
{
    uint16_t a, b = read16(block, addr);
    for (int i = 0; i < 8; i++) {
        a = b;
        b = read16(block, addr);
        if (a == b) break;
    }
    return b;
}

// the chip clears Sn_CR once it has taken the command (a few us)
static bool command(uint8_t cmd)
{
    write8(BLOCK_S0, Sn_CR, cmd);
    for (int i = 0; i < 1000; i++) {
        if (read8(BLOCK_S0, Sn_CR) == 0) return true;
    }
    return false;
}

static bool status_becomes(uint8_t status)
{
    for (int i = 0; i < 100; i++) {
        if (read8(BLOCK_S0, Sn_SR) == status) return true;
    }
    return false;
}

// a cleared Sn_CR means the command was taken, not done: wait for the status too
bool w5500_reopen(void)
{
    for (int attempt = 0; attempt < 2; attempt++) {
        command(Sn_CR_CLOSE);
        status_becomes(SOCK_CLOSED);
        write8(BLOCK_S0, Sn_IR, 0xff);
        write8(BLOCK_S0, Sn_MR, Sn_MR_MACRAW | Sn_MR_MIP6B);
        if (command(Sn_CR_OPEN) && status_becomes(SOCK_MACRAW)) return true;
    }
    return false;
}

int w5500_init(const w5500_cfg_t *c, const uint8_t mac[6])
{
    cfg = *c;
    spi_init(cfg.spi, cfg.baud);
    spi_set_format(cfg.spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(cfg.sck, GPIO_FUNC_SPI);
    gpio_set_function(cfg.mosi, GPIO_FUNC_SPI);
    gpio_set_function(cfg.miso, GPIO_FUNC_SPI);
    gpio_init(cfg.cs);
    gpio_put(cfg.cs, 1);
    gpio_set_dir(cfg.cs, GPIO_OUT);
    gpio_init(cfg.rst);
    gpio_put(cfg.rst, 0);
    gpio_set_dir(cfg.rst, GPIO_OUT);

    vTaskDelay(pdMS_TO_TICKS(2));   // RSTn low for at least 500 us
    gpio_put(cfg.rst, 1);
    int version = -1;
    for (int i = 0; i < 100 && version != VERSION; i++) {
        vTaskDelay(pdMS_TO_TICKS(1));
        version = read8(BLOCK_COMMON, VERSIONR);
    }
    if (version != VERSION) return -1;

    // a software reset too, in case RSTn did nothing (and for a restart without a power cycle)
    write8(BLOCK_COMMON, MR, MR_RST);
    for (int i = 0; i < 100 && (read8(BLOCK_COMMON, MR) & MR_RST); i++) vTaskDelay(pdMS_TO_TICKS(1));

    write_buf(BLOCK_COMMON, SHAR, mac, 6);
    // all 16 KB of each buffer to socket 0: the others first, the total may not pass 16 KB
    for (int n = 7; n >= 0; n--) {
        write8(BLOCK_SOCKET(n), Sn_RXBUF_SIZE, n ? 0 : 16);
        write8(BLOCK_SOCKET(n), Sn_TXBUF_SIZE, n ? 0 : 16);
    }
    // INTn for socket 0 only. Sn_IR only sets the bits Sn_IMR lets through (datasheet; ESP-IDF's
    // W5500 driver enables SEND_OK for this): w5500_tx_commit() waits for SEND_OK
    write8(BLOCK_COMMON, IMR, 0);
    write8(BLOCK_COMMON, SIMR, 0x01);
    write8(BLOCK_S0, Sn_IMR, Sn_IR_RECV | Sn_IR_SENDOK);
    // INTn goes high for ~7 us (256 x 4 PLL clocks) before it asserts again: always an edge
    write16(BLOCK_COMMON, INTLEVEL, 0x0100);
    return w5500_reopen() ? version : -1;
}

bool w5500_socket_ok(void) { return read8(BLOCK_S0, Sn_SR) == SOCK_MACRAW; }

// what w5500_init() set and a chip reset undoes (both 0 and 2 KB afterwards)
bool w5500_settings_ok(void) { return read8(BLOCK_COMMON, SIMR) == 0x01 && read8(BLOCK_S0, Sn_RXBUF_SIZE) == 16; }

void w5500_get_phy(w5500_phy_t *phy)
{
    uint8_t v = read8(BLOCK_COMMON, PHYCFGR);
    phy->link = v & PHY_LNK;
    phy->speed_100 = v & PHY_SPD;
    phy->full_duplex = v & PHY_DPX;
}

void w5500_clear_rx_interrupt(void) { write8(BLOCK_S0, Sn_IR, Sn_IR_RECV); }

// each frame in the buffer: its length + 2 (big endian), then the frame
int w5500_rx_peek(uint8_t *head, size_t head_len)
{
    uint8_t buf[2 + 64];
    if (head_len > sizeof(buf) - 2) head_len = sizeof(buf) - 2;
    uint16_t rsr = read16_stable(BLOCK_S0, Sn_RX_RSR);
    if (rsr < 2) return 0;
    rx_rd = read16(BLOCK_S0, Sn_RX_RD);
    read_buf(BLOCK_S0_RX, rx_rd, buf, 2 + head_len);   // past the frame's end is harmless
    uint16_t size = (uint16_t)(buf[0] << 8 | buf[1]);
    if (size < 2 + 14 || size > 2 + MAX_FRAME) return -1;
    if (size > rsr) return 0;   // not all of it in yet
    rx_len = size - 2;
    memcpy(head, buf + 2, rx_len < head_len ? rx_len : head_len);
    return rx_len;
}

void w5500_rx_read(size_t offset, void *dst, size_t len)
{
    if (len) read_buf(BLOCK_S0_RX, (uint16_t)(rx_rd + 2 + offset), dst, len);
}

void w5500_rx_done(void)
{
    write16(BLOCK_S0, Sn_RX_RD, (uint16_t)(rx_rd + 2 + rx_len));
    command(Sn_CR_RECV);
}

bool w5500_tx_begin(size_t len)
{
    tx_open = false;
    if (len > MAX_FRAME) return false;
    if (read16_stable(BLOCK_S0, Sn_TX_FSR) < (len < MIN_FRAME ? MIN_FRAME : len)) return false;
    tx_wr = read16(BLOCK_S0, Sn_TX_WR);
    tx_len = 0;
    tx_open = true;
    return true;
}

void w5500_tx_append(const void *data, size_t len)
{
    if (!tx_open || !len) return;
    write_buf(BLOCK_S0_TX, (uint16_t)(tx_wr + tx_len), data, len);
    tx_len += len;
}

bool w5500_tx_commit(void)
{
    static const uint8_t zeros[MIN_FRAME];
    if (!tx_open) return false;
    tx_open = false;
    if (tx_len < MIN_FRAME) {
        write_buf(BLOCK_S0_TX, (uint16_t)(tx_wr + tx_len), zeros, MIN_FRAME - tx_len);
        tx_len = MIN_FRAME;
    }
    write16(BLOCK_S0, Sn_TX_WR, (uint16_t)(tx_wr + tx_len));
    if (!command(Sn_CR_SEND)) return false;
    absolute_time_t until = make_timeout_time_ms(10);
    while (!(read8(BLOCK_S0, Sn_IR) & Sn_IR_SENDOK)) {
        if (time_reached(until)) return false;
    }
    write8(BLOCK_S0, Sn_IR, Sn_IR_SENDOK);
    return true;
}
