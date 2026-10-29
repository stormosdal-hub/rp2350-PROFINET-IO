/*
 * config.h - what was "menuconfig → PROFINET IO device" on the ESP32-P4. Change it here, or give a
 * value to CMake: cmake -DPNIO_STATION_NAME=my-io ... (see CMakeLists.txt). The pins are the
 * board's (boards/rp2350_if_v1.h).
 */
#pragma once

/* ---- PROFINET device ---- */

// The PROFINET device name until TIA Portal ("Assign name") or the PLC gives it another, which
// p-net then keeps in flash. Lower case letters, digits, '-' and '.', and it must match the device
// name in the TIA Portal project.
#ifndef PNIO_STATION_NAME
#define PNIO_STATION_NAME "rp2350-io"
#endif

// Shown by TIA Portal as the device type (Accessible devices).
#ifndef PNIO_PRODUCT_NAME
#define PNIO_PRODUCT_NAME "RP2350 PROFINET IO"
#endif

// Empty is what a PROFINET device normally ships with: no address until TIA Portal ("Assign IP
// address") or the PLC (when it connects) sets one by DCP. An address here is only handy for a
// ping before that. Either way p-net keeps what DCP sets.
#ifndef PNIO_IP
#define PNIO_IP ""
#endif
#ifndef PNIO_NETMASK
#define PNIO_NETMASK "255.255.255.0"
#endif
#ifndef PNIO_GATEWAY
#define PNIO_GATEWAY ""
#endif

// The W5500 has no MAC address of its own. Empty: one made from the RP2350's unique ID, locally
// administered (02:xx:xx:xx:xx:xx), the same on every start. Or "02:12:34:56:78:9a", for example.
#ifndef PNIO_MAC
#define PNIO_MAC ""
#endif

/* ---- p-net's sample application: its LEDs and buttons on this board (-1: none) ---- */

// Follows the PLC's output bit 7 of the "DIO 8xLogicLevel" module: 24 V output UT0 (X3-1).
#ifndef PNIO_DATA_LED_GPIO
#define PNIO_DATA_LED_GPIO BOARD_UT0_PIN
#endif

// The input bit 7 to the PLC of the "DIO 8xLogicLevel" module: 24 V input INN0 (X3-3).
#ifndef PNIO_BUTTON1_GPIO
#define PNIO_BUTTON1_GPIO BOARD_INN0_PIN
#endif

// Each press runs the next step of the sample's alarm and diagnosis demo (BOARD_INN1_PIN, say).
#ifndef PNIO_BUTTON2_GPIO
#define PNIO_BUTTON2_GPIO -1
#endif

/* ---- WS2812B LEDs: TIA Portal's "Flash LED" blinks them all; one shows the Ethernet link ---- */

#ifndef PNIO_WS2812_COUNT
#define PNIO_WS2812_COUNT 12
#endif
#ifndef PNIO_WS2812_BRIGHTNESS
#define PNIO_WS2812_BRIGHTNESS 32   // 0..255: 12 LEDs at full white draw ~0.7 A from 5 V
#endif
#ifndef PNIO_LINK_LED
#define PNIO_LINK_LED 0             // which one is green with a link, red without (-1: none)
#endif

/* ---- Ethernet: WIZ850io on SPI0 ---- */

// The RP2350 makes 150 MHz / 2n of it: 33 MHz gives 25 MHz. The W5500 is rated for 33.3 MHz;
// 40 MHz (37.5 MHz) usually works on short wires.
#ifndef PNIO_ETH_SPI_BAUD
#define PNIO_ETH_SPI_BAUD 33000000
#endif

/* ---- Watchdog ---- */

// Fed by the sample application's main loop (every ~11 ms); when it stops for this long the chip
// resets, which also switches the 24 V outputs off. 0: no watchdog
#ifndef PNIO_WATCHDOG_MS
#define PNIO_WATCHDOG_MS 1000
#endif

/* ---- Tasks (FreeRTOS priorities 0..31, stacks in bytes) ---- */

// os_timer (p-net's 1 ms tick) runs at 30, above all of these; lwIP's tcpip thread at 20
#ifndef PNIO_RX_TASK_PRIO
#define PNIO_RX_TASK_PRIO 25        // reading the W5500: PROFINET and LLDP frames to p-net
#endif
#ifndef PNIO_RX_TASK_STACK
#define PNIO_RX_TASK_STACK 6144
#endif
#ifndef PNIO_TASK_PRIO
#define PNIO_TASK_PRIO 24           // the main loop: 1 ms tick, cyclic data
#endif
#ifndef PNIO_TASK_STACK
#define PNIO_TASK_STACK 8192
#endif
#ifndef PNIO_BG_TASK_PRIO
#define PNIO_BG_TASK_PRIO 5         // p-net's background worker: saving to flash, port status
#endif
#ifndef PNIO_BG_TASK_STACK
#define PNIO_BG_TASK_STACK 6144
#endif
#ifndef PNIO_LED_TASK_PRIO
#define PNIO_LED_TASK_PRIO 3
#endif
