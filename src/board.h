/*
 * board.h - the board's I/O for p-net's sample application, and its LEDs.
 */
#pragma once

/* Outputs off, inputs pulled up, the RS-485 driver and the radio off: first thing in main(). */
void board_init(void);

/* The WS2812B LEDs and their task (link status, TIA Portal's "Flash LED"). Call it from a task. */
void board_start(void);

/* Arm the watchdog (PNIO_WATCHDOG_MS): from then on the sample application's main loop feeds it. */
void board_watchdog_start(void);

/* The 24 V outputs off now, by register: for fault handlers. */
void board_outputs_off(void);
