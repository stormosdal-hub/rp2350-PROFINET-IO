/*
 * rp2350_if_v1.h - the Pico SDK board header for the RP2350 IO interface board V1
 * (Wifi_io_if_v1.sch): RP2350A, 12 MHz oscillator, 4 MB W25Q32JV flash, WIZ850io (W5500) Ethernet,
 * RM2 Wi-Fi module, RS-485, RS-232, 4 x 24 V in, 4 x 24 V out, ADS1115 for 4 analog inputs,
 * SSD1306 display, 12 WS2812B LEDs, micro SD.
 *
 * The BOARD_ pins are the MicroPython boardconstants.py of this board.
 */

// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

#ifndef _BOARDS_RP2350_IF_V1_H
#define _BOARDS_RP2350_IF_V1_H

// pico_cmake_set PICO_PLATFORM=rp2350

// For board detection
#define RP2350_IF_V1

// --- RP2350 VARIANT ---
#define PICO_RP2350A 1

// --- UART: the RS-232 port (MAX3232, X2), for a serial console instead of USB ---
#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 1
#endif
#ifndef PICO_DEFAULT_UART_TX_PIN
#define PICO_DEFAULT_UART_TX_PIN 4
#endif
#ifndef PICO_DEFAULT_UART_RX_PIN
#define PICO_DEFAULT_UART_RX_PIN 5
#endif

// --- LED: none for the SDK. GP7 drives LED1 but is also the SD card's chip select ---

// --- I2C: the display ---
#ifndef PICO_DEFAULT_I2C
#define PICO_DEFAULT_I2C 0
#endif
#ifndef PICO_DEFAULT_I2C_SDA_PIN
#define PICO_DEFAULT_I2C_SDA_PIN 16
#endif
#ifndef PICO_DEFAULT_I2C_SCL_PIN
#define PICO_DEFAULT_I2C_SCL_PIN 17
#endif

// --- SPI: the WIZ850io (and the SD card) ---
#ifndef PICO_DEFAULT_SPI
#define PICO_DEFAULT_SPI 0
#endif
#ifndef PICO_DEFAULT_SPI_SCK_PIN
#define PICO_DEFAULT_SPI_SCK_PIN 18
#endif
#ifndef PICO_DEFAULT_SPI_TX_PIN
#define PICO_DEFAULT_SPI_TX_PIN 19
#endif
#ifndef PICO_DEFAULT_SPI_RX_PIN
#define PICO_DEFAULT_SPI_RX_PIN 20
#endif
#ifndef PICO_DEFAULT_SPI_CSN_PIN
#define PICO_DEFAULT_SPI_CSN_PIN 21
#endif

// --- FLASH: W25Q32JVSSIQ, 4 MB ---
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif

// pico_cmake_set_default PICO_FLASH_SIZE_BYTES = (4 * 1024 * 1024)
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (4 * 1024 * 1024)
#endif

// pico_cmake_set_default PICO_RP2350_A2_SUPPORTED = 1
#ifndef PICO_RP2350_A2_SUPPORTED
#define PICO_RP2350_A2_SUPPORTED 1
#endif

// --- the board's own pins (boardconstants.py) ---
#define BOARD_RS485_TX_PIN    0   // UART0, MAX481
#define BOARD_RS485_RX_PIN    1
#define BOARD_RS485_TXEN_PIN  2   // driver enable (DE and /RE): high to send
#define BOARD_ADC_RDY_PIN     3   // ADS1115 ALERT/RDY
#define BOARD_RS232_TX_PIN    4   // UART1, MAX3232
#define BOARD_RS232_RX_PIN    5
#define BOARD_LED_SER_PIN     6   // 12 x WS2812B, through a 5 V level shifter
#define BOARD_LED_PIN         7   // LED1, on when high; also the SD card's chip select (low: selected)
#define BOARD_SD_CS_PIN       7

#define BOARD_INN0_PIN        8   // X3-3: 24 V inputs, opto-isolated (TLP290-4): low while 24 V is on
#define BOARD_INN1_PIN        9   // X3-4
#define BOARD_INN2_PIN       10   // X4-3
#define BOARD_INN3_PIN       11   // X4-4
#define BOARD_UT0_PIN        12   // X3-1: 24 V outputs, high side: high switches 24 V on
#define BOARD_UT1_PIN        13   // X3-2
#define BOARD_UT2_PIN        14   // X4-1
#define BOARD_UT3_PIN        15   // X4-2

#define BOARD_DISPLAY_I2C     0   // SSD1306
#define BOARD_DISPLAY_SDA_PIN 16
#define BOARD_DISPLAY_SCL_PIN 17

#define BOARD_ETH_SPI         0   // WIZ850io: W5500 with RJ45 and magnetics
#define BOARD_ETH_SCK_PIN    18
#define BOARD_ETH_MOSI_PIN   19
#define BOARD_ETH_MISO_PIN   20
#define BOARD_ETH_CS_PIN     21
#define BOARD_ETH_RST_PIN    22
#define BOARD_ETH_INT_PIN    28

#define BOARD_WL_ON_PIN      23   // RM2 Wi-Fi/Bluetooth power (WL_ON and BL_ON)

#define BOARD_ADC_I2C         1   // ADS1115, address 0x48
#define BOARD_ADC_I2C_ADDR 0x48
#define BOARD_ADC_SDA_PIN    26
#define BOARD_ADC_SCL_PIN    27
#define BOARD_AN1_CHANNEL     1   // ADS1115 input of each analog input
#define BOARD_AN2_CHANNEL     0
#define BOARD_AN3_CHANNEL     3
#define BOARD_AN4_CHANNEL     2

#endif
