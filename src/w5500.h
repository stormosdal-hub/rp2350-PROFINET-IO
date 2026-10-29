/*
 * w5500.h - the WIZnet W5500 (on the WIZ850io) as a plain Ethernet MAC: socket 0 in MACRAW mode
 * with all 16 KB of receive and of transmit buffer, the chip's own TCP/IP stack unused.
 *
 * Not thread safe: the caller serialises every call (eth.c holds a mutex around them).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "hardware/spi.h"

typedef struct w5500_cfg {
    spi_inst_t *spi;
    uint32_t baud;   // SPI clock; the RP2350 gives clk_peri / 2n (150 MHz: 37.5, 25, 18.75, ... MHz)
    uint sck, mosi, miso, cs, rst;
} w5500_cfg_t;

typedef struct w5500_phy {
    bool link;
    bool speed_100;     // false: 10 Mbit/s
    bool full_duplex;
} w5500_phy_t;

/* Reset the chip, set the MAC address and open the MACRAW socket. Returns the chip's version
 * register (0x04 for a W5500) or -1 when it does not answer. Waits up to ~100 ms (vTaskDelay). */
int w5500_init(const w5500_cfg_t *cfg, const uint8_t mac[6]);

/* Close and open the MACRAW socket again, dropping what is buffered. */
bool w5500_reopen(void);

/* Whether the MACRAW socket is open (it is not after a chip reset, for example by a brown-out). */
bool w5500_socket_ok(void);

void w5500_get_phy(w5500_phy_t *phy);

/* Clear the "frame received" interrupt (before reading the buffer, so a frame that arrives while
 * it is read asserts INTn again). */
void w5500_clear_rx_interrupt(void);

/*
 * The next received frame: its length (without the W5500's 2-byte header), and its first
 * min(len, head_len) bytes in head. 0 when the buffer is empty, -1 when the buffer is out of step
 * (a bad length: the caller reopens the socket). The frame stays in the buffer until
 * w5500_rx_done(); w5500_rx_read() reads any part of it before that.
 */
int w5500_rx_peek(uint8_t *head, size_t head_len);
void w5500_rx_read(size_t offset, void *dst, size_t len);
void w5500_rx_done(void);

/*
 * Send one frame, from one or more pieces: w5500_tx_begin(total length), w5500_tx_append() for each
 * piece, w5500_tx_commit(). Frames shorter than 60 bytes are padded. The commit waits until the
 * frame is on the wire (SEND_OK, at most ~1.3 ms at 10 Mbit/s). false: too long, no room, or
 * the chip did not confirm.
 */
bool w5500_tx_begin(size_t len);
void w5500_tx_append(const void *data, size_t len);
bool w5500_tx_commit(void);
