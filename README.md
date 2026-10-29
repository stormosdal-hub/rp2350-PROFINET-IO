# RP2350 PROFINET IO device

The RP2350 IO interface board as a PROFINET IO device (RT, Conformance Class A) for a Siemens
S7-1200, so its 24 V inputs and outputs, and later its analog inputs, show up in the PLC as ordinary
`%I`/`%Q` addresses.

This is a port of the ESP32-P4 project
([esp32-p4-profinet-io](https://github.com/stormosdal-hub/esp32-p4-profinet-io)). It runs the
same stack: rt-labs' open-source [p-net](https://github.com/rtlabs-com/p-net) (v0.2.0, GPLv3),
with the same patch. Here it runs on FreeRTOS and lwIP with the Pico SDK, through a port written for
this board (`lib/pnet/port`). Ethernet is the board's WIZ850io (W5500), used as a plain MAC.

## Status

**Ported, and builds with Pico SDK 2.3.1 and 2.2.0. Not yet tested on the board.** The application
is p-net's sample application, unchanged, as on the ESP32-P4, so the GSDML in `gsdml/` and the
TIA Portal project from the ESP32-P4 apply as they are. Only the station name differs
(`rp2350-io`).

To test, as on the ESP32-P4:

1. TIA Portal finds and configures the device (name, IP address, *Flash LED*).
2. Cyclic data with the S7-1200: *DIO 8xLogicLevel* and *Echo Module* at an 8 ms update time.

Next:

3. Our own GSDML and modules: the 4 × 24 V inputs and the 4 × 24 V outputs.
4. The analog inputs (ADS1115) as input modules (`Int`/`Real`).

## Hardware

- The RP2350 IO interface board V1 (`Wifi_io_if_v1.sch`): RP2350A, 4 MB flash, WIZ850io on SPI0.
  The pins are those of `boardconstants.py`, in `boards/rp2350_if_v1.h`.
- An Ethernet switch, so the PC (TIA Portal), the PLC and the board are on one network. The
  1211C has a single PROFINET port. An unmanaged switch is fine.

What the sample application uses on this board (`src/config.h`):

| Sample application | On the board | In the PLC (DIO 8xLogicLevel) |
| --- | --- | --- |
| Button 1 | 24 V input INN0 (X3-3) | input bit 7 |
| Data LED | 24 V output UT0 (X3-1) | output bit 7 |
| Signal LED | all 12 WS2812B LEDs, white | TIA Portal's *Flash LED* |
| Button 2 (alarm demo) | none (`PNIO_BUTTON2_GPIO`) | |

Output bit 7 switches 24 V on X3-1. The outputs are off from reset, and p-net sets the PLC's
outputs to 0 when the connection ends. WS2812B LED 0 is green while the Ethernet link is up and
red without it. LED1 (GP7) is on while the firmware runs. GP7 is also the SD card's chip select,
and high keeps the card off the SPI bus it shares with the W5500.

The RS-485 driver and the RM2 radio are kept off; the display, the analog inputs, the SD card and
the RS-232 port are not used yet.

## Layout

```
CMakeLists.txt          the firmware: src/ + the sample application + lib/pnet
boards/rp2350_if_v1.h   the board for the Pico SDK: RP2350A, 4 MB flash, the pins
config/                 FreeRTOSConfig.h, lwipopts.h
src/
  main.c                starts Ethernet and p-net's sample application
  config.h              what was menuconfig on the ESP32-P4: station name, IP, I/O mapping, tasks
  eth.c                 the WIZ850io as lwIP's interface; PROFINET and LLDP frames to p-net
  w5500.c               the W5500 over SPI: socket 0 in MACRAW mode
  board.c               the sample's LEDs and buttons on the 24 V I/O, the WS2812B LEDs
  app_log_rp2.c         the sample's log, through log_rp2.c (in place of its blocking app_log.c)
lib/pnet/               p-net + OSAL as one Pico SDK library
  p-net/                rt-labs p-net, tag v0.2.0 (src, include, sample_app), with the patches applied
  patches/              our fixes to p-net (see "Changes to p-net")
  osal/                 rt-labs OSAL headers, commit 88784fc as p-net v0.2.0 pins it (untouched)
  port/                 the RP2350 port:
    osal_rp2.c          OSAL on FreeRTOS, the microsecond timer and its alarms for the clock and timers
    log_rp2.c           logging through a ring buffer: never blocks the PROFINET tasks
    pnal_eth_rp2.c      raw Ethernet: PROFINET/LLDP frames from eth.c to the stack, and back out
    pnal_rp2.c          IP suite (lwIP netif, no DHCP), buffers
    pnal_udp_rp2.c      UDP (PNIO-CM, port 34964) on lwIP sockets
    files_rp2.c         the stack's "files" in the last 56 KB of flash
    include/            the headers p-net's CMake would generate (options, version, export)
gsdml/                  the GSDML (and its picture) for TIA Portal
```

How the port works:

- **Receiving.** The W5500's socket 0 is a MACRAW socket with all 16 KB of the chip's receive
  buffer, and it takes every frame on the wire, multicast included. A task (`pn_eth`) reads them
  over SPI, woken by the W5500's interrupt pin (GP28). PROFINET (0x8892) and LLDP (0x88CC)
  frames, also VLAN-tagged as controllers send them, go to p-net. ARP, and IPv4 for this
  device, go to lwIP. Everything else is dropped after reading only its first bytes.
- **Sending.** p-net's frames and lwIP's go through the same SPI bus lock. Frames shorter than
  60 bytes are padded (the W5500 does not).
- **MAC address.** The W5500 has none of its own. The firmware makes one from the RP2350's unique
  ID (02:xx:xx:xx:xx:xx, locally administered), the same at every start, unless `PNIO_MAC` sets one.
- **Tasks.** One core, FreeRTOS. From the highest priority down: `os_timer` (p-net's 1 ms tick),
  `pn_eth` (received frames), `pnio` (the tick and the cyclic data), lwIP's `tcpip`, p-net's
  `p-net_bg_worker` (saving to flash), the LEDs, the log.
- **Storage.** p-net's files (`pnet_data_ip.bin`, `pnet_data_im.bin`, ...) are records in the last
  56 KB of the flash, two 4 KB sectors per file. Each save goes to the sector without the newest
  copy, so a reset during a save keeps the previous one. Flashing new firmware keeps them.
  *Reset to factory settings* in TIA Portal clears them.
- **Running from RAM.** The firmware is copied to RAM at start (`copy_to_ram`): the flash can be
  erased while the PROFINET tasks keep running, and nothing waits for the flash cache. That uses
  about 470 of the 520 KB of RAM.

## Build and flash

You need the Pico SDK 2.1 or later, its ARM toolchain, CMake and Ninja. The easiest way is VS
Code with the *Raspberry Pi Pico* extension, which installs all of them. FreeRTOS is not part of
the SDK. Clone Raspberry Pi's fork of it into `lib/` (git ignores it):

```bash
git clone https://github.com/raspberrypi/FreeRTOS-Kernel.git lib/FreeRTOS-Kernel
```

The build was checked with its commit `4f7299d`. Or set `FREERTOS_KERNEL_PATH` to a clone you have.

In VS Code: *Raspberry Pi Pico → Import Project* on this folder (SDK 2.3.1), then *Compile Project*.
Or, from a terminal with `PICO_SDK_PATH` set:

```bash
cmake -S . -B build -G Ninja
```

```bash
cmake --build build
```

The board is fixed in `CMakeLists.txt` (`PICO_BOARD rp2350_if_v1`). The default build type is
`RelWithDebInfo` (-O2).

To flash: hold *Bootsel*, press *Reset*, and copy `build/rp2350_profinet_io.uf2` to the `RP2350`
drive that appears. Or use the extension's *Run Project*, or `picotool load -x build/rp2350_profinet_io.uf2 -f`.

**Save your MicroPython files first.** Flashing this firmware leaves MicroPython's file system in
place, but p-net's settings go in the last 56 KB of the flash. MicroPython's file system uses that
area too, so it will be damaged if you go back to MicroPython.

The settings are in `src/config.h`:

- station name (default `rp2350-io`)
- an optional starting IP address
- the MAC address (default: made from the chip's unique ID)
- which 24 V input and output the sample application uses
- the WS2812B LEDs: count, brightness, which one shows the link
- the W5500's SPI clock (default 25 MHz)
- task priorities and stacks

The text ones can also be given to CMake, for example `-DPNIO_STATION_NAME=my-io`. The p-net log
level is the CMake variable `PNET_LOG_LEVEL` (0 debug, 1 info, 2 warning, 3 error).

The log is on USB: the board appears as a COM port (USB serial). Any terminal works; the baud rate
does not matter. It shows the start, `eth: link up, ...`, and what p-net logs. To get the log on the
RS-232 port (X2, 115200 baud) instead, change `pico_enable_stdio_usb` / `pico_enable_stdio_uart` in
`CMakeLists.txt`.

## Milestone 1 test in TIA Portal

1. Connect the board, the PLC and the PC to the switch. Flash, and open the COM port. You should see
   `eth: W5500 version 4, ...`, `eth: link up, 100 Mbit/s full duplex, MAC ...` and
   `pnio: p-net running: waiting for TIA Portal or the PLC`. WS2812B LED 0 turns green.
2. In TIA Portal: *Online access → (your PC's Ethernet adapter) → Update accessible devices*. The
   device appears with name `rp2350-io`, its MAC, and IP 0.0.0.0 (or your `PNIO_IP`). Its type is
   "RP2350 PROFINET IO".
3. Open *Online & diagnostics* on it, then *Functions*:
   - **Flash LED** (the *LED flashes* checkbox). TIA Portal sends a DCP *Signal* every 4 s, and each
     one blinks the WS2812B LEDs 3 times. The log shows `board: signal LED on/off`.
   - **Assign IP address.** For example 192.168.0.10 / 255.255.255.0, in the PLC's subnet. The log
     shows `pnet: ... Setting IP` and `eth: IP address ...`, and the device now answers ping.
   - **Assign name.** The new name is kept across resets.
   - **Reset to factory settings.** Name and IP are cleared.

If the device does not show up:

- `eth: the W5500 (WIZ850io) does not answer`: check the WIZ850io's seating and SPI0 (GP18–22).
- No `eth: link up`: the cable, or the switch port.
- In Wireshark on the PC's adapter, filter `pn_dcp || lldp`. You should see TIA Portal's
  *Identify All* request and the device's response.
- In TIA Portal, check that the right adapter is selected under *Online access*. Windows may also
  need the *Siemens PNIO protocol* (PN-IO) enabled on that adapter.

**The log keeps saying `LLDP: Peer info changed`, alternating between the PLC and the PC.** That is
the switch, as on the ESP32-P4: an unmanaged switch passes LLDP on, so the device hears both. It is
harmless as long as the TIA Portal project sets no partner ports for the device. To quiet the log,
build with `-DPNET_LOG_LEVEL=2` (Warning).

## Milestone 2: cyclic data with the S7-1200

As for the ESP32-P4: the same GSDML, the same modules.

1. *Options → Manage general station description files (GSD)*: set the source path to this
   project's `gsdml` folder, tick `GSDML-V2.4-RT-Labs-P-Net-Sample-App-20220324.xml`, and click
   *Install* (already done if you did it for the ESP32-P4).
2. Network view, hardware catalog: *Other field devices → PROFINET IO → I/O → RT-Labs → P-Net
   Samples → P-Net multi-module sample app*. Drag it into the network view. Click its *Not
   assigned* link and choose the PLC's PROFINET interface.
3. In the device's properties, under *PROFINET interface [X1]*:
   - *Ethernet addresses*: an IP address in the PLC's subnet. Untick *Generate PROFINET device name
     automatically* and type the device's station name (`rp2350-io`, or use *Assign device name*
     after the download).
   - *Advanced options → Real time settings → IO cycle*: *Update time* set manually to 8 ms to start
     with, and leave the watchdog at 3 cycles.
4. Device view: plug *DIO 8xLogicLevel* into slot 1 and *Echo Module* into slot 2.
5. Compile and download the hardware configuration. Within a few seconds of the PLC starting, it
   sets the device's IP by DCP and connects. The PLC's error LED stops flashing, and the log shows
   the connect, PRM_END and *Application ready* steps.

What to expect, in a watch table (addresses as TIA Portal gave them on a 1211C for the ESP32-P4):

| Address | Display format | What it is |
| --- | --- | --- |
| `%IB1` | DEC_unsigned | counter 0..127 (+1 every 100 ms), +128 while 24 V is on INN0 |
| `%Q1.7` | Bool | 24 V output UT0 (X3-1); the log shows `board: data LED on/off` |
| `%QD64` → `%ID68` | Floating-point | echo REAL out → back (× gain) |
| `%QD68` → `%ID72` | DEC_unsigned | echo UDInt out → back (× gain) |

The PLC must be in RUN to drive outputs: in STOP they are 0.

**"IO device failure" with the device's HW_ID, and the connect repeats every few seconds.** The
device's cyclic frames stopped for longer than the PLC's watchdog (3 update cycles). Look in the log
for `log: N lines dropped` and for anything else that keeps the CPU busy. A shorter update time than
8 ms works only if the 1 ms tick and the frames keep up: the RP2350 has one core for all of it, and
every frame crosses SPI.

## Changes to p-net

`lib/pnet/p-net` is the v0.2.0 tag (`src`, `include` and `sample_app`, without the ports and
tests) plus the patches in `lib/pnet/patches`, already applied. After copying p-net again, run
`patch -p1 < ../patches/0001-pdport-peer-check-exact-port-name.patch` inside `p-net/`.

- **`0001-pdport-peer-check-exact-port-name.patch`** (from the ESP32-P4 project). The topology
  check (the PLC's PDPortDataCheck, when a partner port is set in TIA Portal) compared the
  neighbour's whole LLDP port ID with the expected port name. An S7-1200 sends `port-001.plc`, so
  the check always failed with "Wrong partner port". It now compares the port name only, and both
  name checks need an exact match.

## Topology: partner ports in TIA Portal

As on the ESP32-P4: only set a partner port when the cable really runs device ↔ partner, or through
a PROFINET switch that is in the project too. Behind an unmanaged switch set the partner to *Any
partner*.

## Licenses

- p-net is GPLv3, or commercial from rt-labs. GPLv3 is fine for personal and open-source use. If
  you distribute firmware built with it, its source has to come along. A commercial product needs
  rt-labs' commercial license and PROFINET certification.
- The rt-labs OSAL is BSD-3-Clause. FreeRTOS is MIT, the Pico SDK and lwIP BSD-3-Clause.
- `src/ws2812.pio` is from pico-examples (BSD-3-Clause).
- The GSDML uses rt-labs' sample vendor and device IDs (0x0493 / 0x0002). That is fine for a lab
  setup; a product needs its own vendor ID from PROFIBUS & PROFINET International.

This is not a safety device: do not use it for safety functions.
