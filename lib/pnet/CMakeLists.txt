# rt-labs p-net (PROFINET device stack, v0.2.0, GPLv3) and rt-labs OSAL (BSD-3), as one Pico SDK
# style library: an INTERFACE library, built with the executable that links it (as the SDK's own).
# Everything RP2350 is in port/ (OSAL on FreeRTOS + pico_time, PNAL on lwIP, the Ethernet driver's
# raw frames and the flash). The upstream trees in p-net/ and osal/ are as rt-labs ships them,
# plus the bug fixes in patches/ (already applied).
set(PNET_LOG_LEVEL 1 CACHE STRING "p-net log level: 0 debug, 1 info, 2 warning, 3 error, 4 fatal")

set(pn ${CMAKE_CURRENT_LIST_DIR}/p-net/src)
set(port ${CMAKE_CURRENT_LIST_DIR}/port)

add_library(pnet INTERFACE)
target_sources(pnet INTERFACE
    ${pn}/device/pnet_api.c
    ${pn}/device/pf_block_reader.c
    ${pn}/device/pf_block_writer.c
    ${pn}/device/pf_fspm.c
    ${pn}/device/pf_diag.c
    ${pn}/device/pf_cmdev.c
    ${pn}/device/pf_cmdmc.c
    ${pn}/device/pf_cmina.c
    ${pn}/device/pf_cmio.c
    ${pn}/device/pf_cmpbe.c
    ${pn}/device/pf_cmrdr.c
    ${pn}/device/pf_cmrpc.c
    ${pn}/device/pf_cmrpc_epm.c
    ${pn}/device/pf_cmrpc_helpers.c
    ${pn}/device/pf_cmrs.c
    ${pn}/device/pf_cmsm.c
    ${pn}/device/pf_cmsu.c
    ${pn}/device/pf_cmwrr.c
    ${pn}/device/pf_pdport.c
    ${pn}/device/pf_port.c
    ${pn}/common/pf_alarm.c
    ${pn}/common/pf_bg_worker.c
    ${pn}/common/pf_cpm.c
    ${pn}/common/pf_dcp.c
    ${pn}/common/pf_ppm.c
    ${pn}/common/pf_ptcp.c
    ${pn}/common/pf_scheduler.c
    ${pn}/common/pf_eth.c
    ${pn}/common/pf_file.c
    ${pn}/common/pf_lldp.c
    ${pn}/common/pf_udp.c
    ${port}/osal_rp2.c
    ${port}/log_rp2.c
    ${port}/pnal_rp2.c
    ${port}/pnal_eth_rp2.c
    ${port}/pnal_udp_rp2.c
    ${port}/files_rp2.c
)
target_include_directories(pnet INTERFACE
    ${port}/include
    ${port}/osal
    ${CMAKE_CURRENT_LIST_DIR}/p-net/include
    ${pn}
    ${pn}/common
    ${pn}/device
    ${CMAKE_CURRENT_LIST_DIR}/osal/include
)
target_compile_definitions(pnet INTERFACE
    PNET_LOG_LEVEL=${PNET_LOG_LEVEL}
    # lwIP's own core lock (a FreeRTOS mutex), not the one pico_cyw43_arch provides
    PICO_LWIP_CUSTOM_LOCK_TCPIP_CORE=0
)
# lwIP on FreeRTOS without pico_async_context: its contrib sys_arch, our lwipopts.h
target_link_libraries(pnet INTERFACE
    pico_stdlib
    pico_time
    hardware_flash
    hardware_sync
    pico_lwip
    pico_lwip_contrib_freertos
    FreeRTOS-Kernel-Heap4
)
