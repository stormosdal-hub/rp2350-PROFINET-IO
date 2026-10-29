/*
 * main.c - a PROFINET IO device on the RP2350: the RJ45 (eth.c, a WIZ850io) with rt-labs' p-net
 * stack on it (lib/pnet), running p-net's own sample application for now. That application
 * matches gsdml/GSDML-V2.4-RT-Labs-P-Net-Sample-App-20220324.xml (vendor 0x0493, device 0x0002).
 *
 * TIA Portal finds the device (Online access > Update accessible devices), can flash its LEDs,
 * and can give it a name and an IP address; the PLC exchanges cyclic data with it.
 */
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdlib.h"
#include "app_log.h"
#include "app_utils.h"
#include "sampleapp_common.h"
#include "pnet_version.h"
#include "pnal_rp2.h"
#include "board.h"
#include "config.h"
#include "eth.h"
#include "log.h"

static const char *TAG = "pnio";

// p-net keeps pointers into these: they live as long as the device runs
static pnet_cfg_t pnet_cfg;
static app_utils_netif_namelist_t netifs;
static app_args_t app_args;

// the stack's tick (APP_TICK_INTERVAL_US, 1 ms) and the sample application's cyclic data
static void pnio_task(void *arg)
{
    pnet_if_cfg_t if_cfg = { 0 };
    uint16_t ports = 0;

    snprintf(app_args.eth_interfaces, sizeof(app_args.eth_interfaces), "%s", PNAL_RP2_NETIF_NAME);
    snprintf(app_args.station_name, sizeof(app_args.station_name), "%s", PNIO_STATION_NAME);
    // Debug: the sample's connection events (connect, PRM_END, application ready, data, abort);
    // nothing at this level comes every cycle
    app_log_set_log_level(APP_LOG_LEVEL_DEBUG);

    app_pnet_cfg_init_default(&pnet_cfg);
    snprintf(pnet_cfg.station_name, sizeof(pnet_cfg.station_name), "%s", app_args.station_name);
    snprintf(pnet_cfg.product_name, sizeof(pnet_cfg.product_name), "%s", PNIO_PRODUCT_NAME);
    pnet_cfg.file_directory[0] = '\0';   // the "files" are flash records (lib/pnet/port/files_rp2.c)

    if (app_utils_pnet_cfg_init_netifs(app_args.eth_interfaces, &netifs, &ports, &if_cfg) != 0) {
        LOGE(TAG, "network interface \"%s\" not usable", app_args.eth_interfaces);
        vTaskDelete(NULL);
    }
    pnet_cfg.if_cfg = if_cfg;
    pnet_cfg.num_physical_ports = ports;
    app_utils_print_network_config(&if_cfg, ports);

    pnet_cfg.pnal_cfg.bg_worker_thread = (pnal_thread_cfg_t){ .prio = PNIO_BG_TASK_PRIO,
                                                              .stack_size = PNIO_BG_TASK_STACK };

    app_data_t *app = app_init(&pnet_cfg);
    if (!app || app_start(app, RUN_IN_MAIN_THREAD) != 0) {
        LOGE(TAG, "p-net did not start");
        vTaskDelete(NULL);
    }
    LOGI(TAG, "p-net running: waiting for TIA Portal or the PLC");   // the name in use: CMINA "Setting IP" above
    app_loop_forever(app);   // never returns
}

// what ESP-IDF's app_main() did: everything that needs the scheduler running
static void start_task(void *arg)
{
    LOGI(TAG, "RP2350 PROFINET IO device, p-net %s, FreeRTOS %s", PNET_VERSION, tskKERNEL_VERSION_NUMBER);
    board_start();
    if (eth_start() != 0) {
        LOGE(TAG, "no Ethernet: PROFINET not started");
        vTaskDelete(NULL);
    }
    if (xTaskCreate(pnio_task, "pnio", PNIO_TASK_STACK / sizeof(StackType_t), NULL, PNIO_TASK_PRIO, NULL) != pdPASS) {
        LOGE(TAG, "pnio task could not be created");
    }
    vTaskDelete(NULL);
}

int main(void)
{
    board_init();   // first: the 24 V outputs off
    stdio_init_all();
    // from here on no log line waits for USB (it would stall the cyclic frames)
    pnal_rp2_log_start();
    xTaskCreate(start_task, "start", 2048, NULL, PNIO_TASK_PRIO, NULL);
    vTaskStartScheduler();   // never returns
    return 0;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    panic("stack overflow in task %s", name);
}

void vApplicationMallocFailedHook(void)
{
    LOGE("rtos", "out of memory (FreeRTOS heap, configTOTAL_HEAP_SIZE)");
}
