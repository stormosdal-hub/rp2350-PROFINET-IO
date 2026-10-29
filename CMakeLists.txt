# RP2350 PROFINET IO device: rt-labs' p-net on FreeRTOS and lwIP, the RJ45 a WIZ850io (W5500).
cmake_minimum_required(VERSION 3.13)

# The board: boards/rp2350_if_v1.h (RP2350A, 4 MB flash, the pins of boardconstants.py)
set(PICO_BOARD rp2350_if_v1 CACHE STRING "Board type")
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)

# -O2 (the SDK's Release is -O3: a quarter more code, and all of it is copied to RAM)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type")
endif()

# The Pico SDK (before project()): PICO_SDK_PATH, or the one the VS Code extension installed
include(pico_sdk_import.cmake)

project(rp2350_profinet_io C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "2.1.0")
    message(FATAL_ERROR "Pico SDK 2.1.0 or later required, this is ${PICO_SDK_VERSION_STRING}")
endif()

pico_sdk_init()

# FreeRTOS: Raspberry Pi's fork of the kernel (it has the RP2350 port). FREERTOS_KERNEL_PATH (CMake
# or environment), else a clone in lib/FreeRTOS-Kernel, else one next to the Pico SDK
if (NOT FREERTOS_KERNEL_PATH AND NOT DEFINED ENV{FREERTOS_KERNEL_PATH})
    if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/lib/FreeRTOS-Kernel/tasks.c)
        set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/FreeRTOS-Kernel)
    elseif (NOT EXISTS ${PICO_SDK_PATH}/../FreeRTOS-Kernel/tasks.c)
        message(FATAL_ERROR "FreeRTOS-Kernel not found. In this folder run:\n"
            "  git clone https://github.com/raspberrypi/FreeRTOS-Kernel.git lib/FreeRTOS-Kernel\n"
            "or set FREERTOS_KERNEL_PATH to a clone of it.")
    endif()
endif()
include(FreeRTOS_Kernel_import.cmake)

add_subdirectory(lib/pnet)

# p-net's sample application is built from the vendored tree, unchanged: it is the device that
# gsdml/GSDML-V2.4-RT-Labs-P-Net-Sample-App-20220324.xml describes. Our own modules (24 V I/O,
# analog inputs) replace app_gsdml.c and app_data.c in a later milestone.
set(sample ${CMAKE_CURRENT_LIST_DIR}/lib/pnet/p-net/sample_app)

# app_log_rp2.c stands in for the sample's app_log.c, whose printing blocks.
add_executable(rp2350_profinet_io
    src/main.c
    src/eth.c
    src/w5500.c
    src/board.c
    src/app_log_rp2.c
    ${sample}/sampleapp_common.c
    ${sample}/app_utils.c
    ${sample}/app_gsdml.c
    ${sample}/app_data.c
)
# config/: FreeRTOSConfig.h and lwipopts.h, for FreeRTOS, lwIP and p-net's port too
target_include_directories(rp2350_profinet_io PRIVATE src config ${sample})
pico_generate_pio_header(rp2350_profinet_io ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

# The settings of src/config.h that are text can come from here too: cmake -DPNIO_STATION_NAME=my-io
foreach(setting PNIO_STATION_NAME PNIO_PRODUCT_NAME PNIO_IP PNIO_NETMASK PNIO_GATEWAY PNIO_MAC)
    if (DEFINED ${setting})
        target_compile_definitions(rp2350_profinet_io PRIVATE ${setting}="${${setting}}")
    endif()
endforeach()

target_link_libraries(rp2350_profinet_io PRIVATE
    pnet
    pico_stdlib
    pico_unique_id
    hardware_gpio
    hardware_irq
    hardware_pio
    hardware_spi
)

# The log goes to USB (a COM port); stdio over the RS-232 port instead: PICO_STDIO_UART=1
pico_enable_stdio_usb(rp2350_profinet_io 1)
pico_enable_stdio_uart(rp2350_profinet_io 0)

# All of it runs from RAM: nothing runs from flash while p-net saves to it (files_rp2.c), so the
# PROFINET tasks keep running then, and the cyclic path never waits for the flash cache
pico_set_binary_type(rp2350_profinet_io copy_to_ram)

pico_add_extra_outputs(rp2350_profinet_io)
