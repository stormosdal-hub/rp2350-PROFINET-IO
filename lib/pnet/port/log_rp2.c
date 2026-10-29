/*
 * log_rp2.c - logging that never blocks the PROFINET tasks. stdio waits for its output: over USB
 * until the host has taken it (up to 500 ms when the host does not read), over a UART at 115200
 * baud ~13 ms for a 150 character line. Two or three lines in the task that sends the cyclic
 * frames and the PLC's watchdog (3 update cycles) expires. So lines go into a ring buffer, and a
 * low-priority task writes them out. When the buffer is full a line is dropped and counted, never
 * waited for.
 */
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "pico/time.h"
#include "pnal_rp2.h"

#ifndef PNAL_RP2_LOG_BUFFER_SIZE
#define PNAL_RP2_LOG_BUFFER_SIZE 16384   // a connection to the PLC logs about 4 KB at Info level
#endif
#ifndef PNAL_RP2_LOG_TASK_PRIO
#define PNAL_RP2_LOG_TASK_PRIO 2
#endif

#define LINE_MAX_LEN 256

_Static_assert((PNAL_RP2_LOG_BUFFER_SIZE & (PNAL_RP2_LOG_BUFFER_SIZE - 1)) == 0,
               "PNAL_RP2_LOG_BUFFER_SIZE: a power of two");

static char ring[PNAL_RP2_LOG_BUFFER_SIZE];
static uint32_t wr, rd;   // free-running: wr - rd bytes are waiting
static uint32_t dropped;
static TaskHandle_t log_task;

static void copy_in(const char *src, size_t len)
{
    size_t at = wr & (PNAL_RP2_LOG_BUFFER_SIZE - 1);
    size_t first = PNAL_RP2_LOG_BUFFER_SIZE - at;
    if (first > len) first = len;
    memcpy(ring + at, src, first);
    memcpy(ring, src + first, len - first);
    wr += len;
}

static size_t copy_out(char *dst, size_t max)
{
    size_t len = wr - rd;
    if (len > max) len = max;
    size_t at = rd & (PNAL_RP2_LOG_BUFFER_SIZE - 1);
    size_t first = PNAL_RP2_LOG_BUFFER_SIZE - at;
    if (first > len) first = len;
    memcpy(dst, ring + at, first);
    memcpy(dst + first, ring, len - first);
    rd += len;
    return len;
}

static void log_task_fn(void *arg)
{
    char chunk[128];
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        for (;;) {
            taskENTER_CRITICAL();
            size_t n = copy_out(chunk, sizeof(chunk));
            taskEXIT_CRITICAL();
            if (!n) break;
            fwrite(chunk, 1, n, stdout);
        }
        taskENTER_CRITICAL();
        uint32_t n = dropped;
        dropped = 0;
        taskEXIT_CRITICAL();
        if (n) printf("W (%lu) log: %lu lines dropped (log buffer full)\n",
                      (unsigned long)to_ms_since_boot(get_absolute_time()), (unsigned long)n);
        fflush(stdout);
    }
}

// before pnal_rp2_log_start(), if it failed, and before the scheduler runs: straight out. (A
// critical section before vTaskStartScheduler() would leave the interrupts masked until it.)
void pnal_rp2_log_write(const char *line, size_t len)
{
    if (!log_task || xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        fwrite(line, 1, len, stdout);
        return;
    }
    bool queued;
    if (portCHECK_IF_IN_ISR()) {
        UBaseType_t saved = taskENTER_CRITICAL_FROM_ISR();
        queued = len <= PNAL_RP2_LOG_BUFFER_SIZE - (wr - rd);
        if (queued) copy_in(line, len);
        else dropped++;
        taskEXIT_CRITICAL_FROM_ISR(saved);
        if (queued) {
            BaseType_t woken = pdFALSE;
            vTaskNotifyGiveFromISR(log_task, &woken);
            portYIELD_FROM_ISR(woken);
        }
        return;
    }
    taskENTER_CRITICAL();
    queued = len <= PNAL_RP2_LOG_BUFFER_SIZE - (wr - rd);
    if (queued) copy_in(line, len);
    else dropped++;
    taskEXIT_CRITICAL();
    if (queued) xTaskNotifyGive(log_task);
}

// a line too long for the buffer is cut, and keeps its newline
int pnal_rp2_log_vprintf(const char *fmt, va_list ap)
{
    char line[LINE_MAX_LEN];
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    if (n <= 0) return n;
    size_t len = (size_t)n;
    if (len >= sizeof(line)) {
        len = sizeof(line) - 1;
        line[len - 1] = '\n';
    }
    pnal_rp2_log_write(line, len);
    return n;
}

void pnal_rp2_logf(char level, const char *tag, const char *fmt, ...)
{
    char line[LINE_MAX_LEN];
    int n = snprintf(line, sizeof(line), "%c (%lu) %s: ", level, (unsigned long)to_ms_since_boot(get_absolute_time()),
                     tag);
    if (n < 0) return;
    if ((size_t)n > sizeof(line) - 2) n = sizeof(line) - 2;
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(line + n, sizeof(line) - 1 - n, fmt, ap);   // room for the newline
    va_end(ap);
    size_t len = (size_t)n + (m > 0 ? (size_t)m : 0);
    if (len > sizeof(line) - 2) len = sizeof(line) - 2;
    line[len++] = '\n';
    pnal_rp2_log_write(line, len);
}

int pnal_rp2_log_start(void)
{
    if (log_task) return 0;
    TaskHandle_t t = NULL;
    if (xTaskCreate(log_task_fn, "pn_log", 1024, NULL, PNAL_RP2_LOG_TASK_PRIO, &t) != pdPASS) return -1;
    log_task = t;
    return 0;
}
