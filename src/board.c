/*
 * board.c - what p-net's sample application asks of a board (sampleapp_common.h): two LEDs and two
 * buttons. Here the data LED (the PLC's output bit 7) is a 24 V output and button 1 (an input bit
 * to the PLC) a 24 V input, as config.h chooses. The signal LED, which TIA Portal's "Flash LED"
 * blinks, is the WS2812B chain: all of them flash white. Both LEDs are also logged.
 */
#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/structs/sio.h"
#include "hardware/watchdog.h"
#include "config.h"
#include "eth.h"
#include "log.h"
#include "sampleapp_common.h"
#include "ws2812.pio.h"
#include "board.h"

static const char *TAG = "board";

#define LED_POLL_MS 200   // how soon the link LED follows the cable

static const uint outputs[] = { BOARD_UT0_PIN, BOARD_UT1_PIN, BOARD_UT2_PIN, BOARD_UT3_PIN };
static const uint inputs[] = { BOARD_INN0_PIN, BOARD_INN1_PIN, BOARD_INN2_PIN, BOARD_INN3_PIN };

static PIO pio;
static uint sm;
static TaskHandle_t led_task;
static volatile bool signal_on;
static volatile bool watchdog_on;

// set the level before the direction: no glitch on a 24 V output
static void output(int pin, bool level)
{
    if (pin < 0) return;
    gpio_init((uint)pin);
    gpio_put((uint)pin, level);
    gpio_set_dir((uint)pin, GPIO_OUT);
}

// the opto-couplers and buttons pull the pin to GND
static void input(int pin)
{
    if (pin < 0) return;
    gpio_init((uint)pin);
    gpio_set_dir((uint)pin, GPIO_IN);
    gpio_pull_up((uint)pin);
}

void board_outputs_off(void)
{
    const int data_pin = PNIO_DATA_LED_GPIO;
    for (size_t i = 0; i < sizeof(outputs) / sizeof(outputs[0]); i++) sio_hw->gpio_clr = 1u << outputs[i];
    if (data_pin >= 0) gpio_put((uint)data_pin, false);
}

void board_init(void)
{
    for (size_t i = 0; i < sizeof(outputs) / sizeof(outputs[0]); i++) output((int)outputs[i], false);
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) input((int)inputs[i]);
    output(PNIO_DATA_LED_GPIO, false);
    input(PNIO_BUTTON1_GPIO);
    input(PNIO_BUTTON2_GPIO);
    output(BOARD_RS485_TXEN_PIN, false);   // MAX481 driver off, receiver on
    output(BOARD_RS232_TX_PIN, true);      // MAX3232 T1IN idle (mark), not a break; stdio may take it
    output(BOARD_WL_ON_PIN, false);        // RM2 radio off
    output(BOARD_LED_PIN, true);           // LED1 on; also the SD card's CS: high keeps it off SPI0
    output(BOARD_ETH_CS_PIN, true);        // W5500 deselected until eth_start()
}

static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    r = (uint8_t)(r * PNIO_WS2812_BRIGHTNESS / 255);
    g = (uint8_t)(g * PNIO_WS2812_BRIGHTNESS / 255);
    b = (uint8_t)(b * PNIO_WS2812_BRIGHTNESS / 255);
    return (uint32_t)g << 16 | (uint32_t)r << 8 | b;   // the WS2812B takes green, red, blue
}

static void led_task_fn(void *arg)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(LED_POLL_MS));
        bool flash = signal_on, link = eth_link_up();
        for (int i = 0; i < PNIO_WS2812_COUNT; i++) {
            uint32_t colour = 0;
            if (flash) colour = rgb(255, 255, 255);
            else if (i == PNIO_LINK_LED) colour = link ? rgb(0, 255, 0) : rgb(255, 0, 0);
            pio_sm_put_blocking(pio, sm, colour << 8u);
        }
        // up to 9 LEDs (~270 us) still go out of the FIFO; then data low > 280 us latches them.
        // Two ticks: at least one whole millisecond
        vTaskDelay(2);
    }
}

void board_start(void)
{
    uint offset;
    if (PNIO_WS2812_COUNT <= 0) return;
    if (!pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio, &sm, &offset, BOARD_LED_SER_PIN, 1,
                                                          true)) {
        LOGW(TAG, "no free PIO state machine: WS2812B LEDs unused");
        return;
    }
    ws2812_program_init(pio, sm, offset, BOARD_LED_SER_PIN, 800000, false);
    // the pin drives the level shifter's MOSFET (AO3400): sharp edges keep the pulse widths
    gpio_set_drive_strength(BOARD_LED_SER_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(BOARD_LED_SER_PIN, GPIO_SLEW_RATE_FAST);
    if (xTaskCreate(led_task_fn, "led", 1024, NULL, PNIO_LED_TASK_PRIO, &led_task) != pdPASS) {
        LOGW(TAG, "LED task could not be created");
    }
}

void board_watchdog_start(void)
{
    if (PNIO_WATCHDOG_MS <= 0) return;
    watchdog_enable(PNIO_WATCHDOG_MS, true);
    watchdog_on = true;
}

void app_set_led(uint16_t id, bool led_state)
{
    static bool on[3];   // by id: APP_DATA_LED_ID 1, APP_PROFINET_SIGNAL_LED_ID 2
    const int data_pin = PNIO_DATA_LED_GPIO;
    if (id == APP_DATA_LED_ID && data_pin >= 0) gpio_put((uint)data_pin, led_state);
    if (id == APP_PROFINET_SIGNAL_LED_ID) {
        signal_on = led_state;
        if (led_task) xTaskNotifyGive(led_task);
    }
    if (id < 3 && led_state != on[id]) {
        on[id] = led_state;
        LOGI(TAG, "%s LED %s", id == APP_DATA_LED_ID ? "data" : "signal", led_state ? "on" : "off");
    }
}

// id 0: button 1 (an input bit to the PLC), id 1: button 2 (the sample's alarm demo). The sample's
// main loop reads them every 11 ticks: proof that it and p-net's tick run, so the watchdog is fed here
bool app_get_button(uint16_t id)
{
    int pin = id == 0 ? PNIO_BUTTON1_GPIO : id == 1 ? PNIO_BUTTON2_GPIO : -1;
    if (id == 0 && watchdog_on) watchdog_update();
    return pin >= 0 && !gpio_get((uint)pin);
}
