/*
 * osal_rp2.c - rt-labs OSAL (osal.h, osal_log.h) on the RP2350. FreeRTOS for tasks and their
 * synchronisation, the microsecond timer for the clock, and its hardware alarms (pico_time's
 * default alarm pool) for the timers: p-net's scheduler counts in microseconds, which FreeRTOS
 * ticks (1 ms at best) cannot give it.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "hardware/timer.h"
#include "pico/time.h"
#include "osal.h"
#include "osal_log.h"
#include "pnal_rp2.h"

// timer callbacks run in this task, above every other PROFINET task (as in esp_timer's)
#ifndef OS_TIMER_TASK_PRIO
#define OS_TIMER_TASK_PRIO (configMAX_PRIORITIES - 2)
#endif
#ifndef OS_TIMER_TASK_STACK
#define OS_TIMER_TASK_STACK 3072   // bytes
#endif

// a timeout in ms as ticks, rounded up: a short wait must not become no wait at all
static TickType_t ticks(uint32_t ms)
{
    if (ms == OS_WAIT_FOREVER) return portMAX_DELAY;
    TickType_t t = pdMS_TO_TICKS(ms);
    return (t == 0 && ms > 0) ? 1 : t;
}

// the FreeRTOS heap (heap_4): thread safe, and what p-net allocates stays out of newlib's heap
void *os_malloc(size_t size) { return pvPortMalloc(size); }
void os_free(void *ptr) { vPortFree(ptr); }

void os_usleep(uint32_t us)
{
    TickType_t t = (TickType_t)(((uint64_t)us * configTICK_RATE_HZ + 999999) / 1000000);
    vTaskDelay(t ? t : 1);
}

uint32_t os_get_current_time_us(void) { return time_us_32(); }

// stack size in bytes, as p-net gives it; FreeRTOS counts in stack words
os_thread_t *os_thread_create(const char *name, uint32_t priority, size_t stacksize,
                              void (*entry)(void *arg), void *arg)
{
    TaskHandle_t task = NULL;
    if (priority >= configMAX_PRIORITIES) priority = configMAX_PRIORITIES - 1;
    configSTACK_DEPTH_TYPE words = (configSTACK_DEPTH_TYPE)((stacksize + sizeof(StackType_t) - 1) / sizeof(StackType_t));
    if (xTaskCreate(entry, name, words, arg, priority, &task) != pdPASS) return NULL;
    return task;
}

os_mutex_t *os_mutex_create(void) { return xSemaphoreCreateRecursiveMutex(); }
void os_mutex_lock(os_mutex_t *mutex) { xSemaphoreTakeRecursive(mutex, portMAX_DELAY); }
void os_mutex_unlock(os_mutex_t *mutex) { xSemaphoreGiveRecursive(mutex); }
void os_mutex_destroy(os_mutex_t *mutex) { vSemaphoreDelete(mutex); }

os_sem_t *os_sem_create(size_t count) { return xSemaphoreCreateCounting(INT32_MAX, count); }

// true: timed out
bool os_sem_wait(os_sem_t *sem, uint32_t time) { return xSemaphoreTake(sem, ticks(time)) != pdTRUE; }
void os_sem_signal(os_sem_t *sem) { xSemaphoreGive(sem); }
void os_sem_destroy(os_sem_t *sem) { vSemaphoreDelete(sem); }

os_event_t *os_event_create(void) { return xEventGroupCreate(); }

// true: timed out. The bits stay set: the caller clears what it handled (os_event_clr)
bool os_event_wait(os_event_t *event, uint32_t mask, uint32_t *value, uint32_t time)
{
    *value = xEventGroupWaitBits(event, mask, pdFALSE, pdFALSE, ticks(time)) & mask;
    return *value == 0;
}

void os_event_set(os_event_t *event, uint32_t value) { xEventGroupSetBits(event, value); }
void os_event_clr(os_event_t *event, uint32_t value) { xEventGroupClearBits(event, value); }
void os_event_destroy(os_event_t *event) { vEventGroupDelete(event); }

os_mbox_t *os_mbox_create(size_t size) { return xQueueCreate(size, sizeof(void *)); }

// true: timed out
bool os_mbox_fetch(os_mbox_t *mbox, void **msg, uint32_t time) { return xQueueReceive(mbox, msg, ticks(time)) != pdTRUE; }
bool os_mbox_post(os_mbox_t *mbox, void *msg, uint32_t time) { return xQueueSendToBack(mbox, &msg, ticks(time)) != pdTRUE; }
void os_mbox_destroy(os_mbox_t *mbox) { vQueueDelete(mbox); }

/*
 * Timers. The alarm interrupt only marks the timer due and wakes the timer task, which runs the
 * callbacks: they may then use every FreeRTOS call (the sample's sets an event bit). A periodic
 * timer that is late skips the ticks it missed instead of making them up in a burst.
 */
static TaskHandle_t timer_task;
static SemaphoreHandle_t timers_lock;   // recursive: a callback may start or stop timers
static os_timer_t *timers;

// the alarm interrupt (default IRQ priority: below configMAX_SYSCALL_INTERRUPT_PRIORITY)
static int64_t on_alarm(alarm_id_t id, void *user_data)
{
    os_timer_t *t = user_data;
    BaseType_t woken = pdFALSE;
    t->due = true;
    vTaskNotifyGiveFromISR(timer_task, &woken);
    portYIELD_FROM_ISR(woken);
    if (t->oneshot) {
        t->alarm = 0;
        return 0;
    }
    uint64_t next = t->at + (t->us ? t->us : 1);
    uint64_t now = time_us_64();
    if (next <= now) next = now + (t->us ? t->us : 1);   // late: skip what was missed
    int64_t ret = -(int64_t)(next - t->at);               // negative: relative to when this one was due
    t->at = next;
    return ret;
}

static void timer_task_fn(void *arg)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        xSemaphoreTakeRecursive(timers_lock, portMAX_DELAY);
        for (os_timer_t *t = timers; t;) {
            os_timer_t *next = t->next;
            if (t->due) {
                t->due = false;
                if (t->fn) t->fn(t, t->arg);
            }
            t = next;
        }
        xSemaphoreGiveRecursive(timers_lock);
    }
}

static void cancel(os_timer_t *t)
{
    taskENTER_CRITICAL();
    if (t->alarm > 0) cancel_alarm(t->alarm);
    t->alarm = 0;
    t->due = false;
    taskEXIT_CRITICAL();
}

os_timer_t *os_timer_create(uint32_t us, void (*fn)(os_timer_t *timer, void *arg), void *arg, bool oneshot)
{
    if (!timer_task) {
        timers_lock = xSemaphoreCreateRecursiveMutex();
        if (!timers_lock) return NULL;
        if (xTaskCreate(timer_task_fn, "os_timer", OS_TIMER_TASK_STACK / sizeof(StackType_t), NULL,
                        OS_TIMER_TASK_PRIO, &timer_task) != pdPASS) {
            vSemaphoreDelete(timers_lock);
            timers_lock = NULL;
            timer_task = NULL;
            return NULL;
        }
    }
    os_timer_t *timer = pvPortMalloc(sizeof(*timer));
    if (!timer) return NULL;
    memset(timer, 0, sizeof(*timer));
    timer->fn = fn;
    timer->arg = arg;
    timer->us = us;
    timer->oneshot = oneshot;
    xSemaphoreTakeRecursive(timers_lock, portMAX_DELAY);
    timer->next = timers;
    timers = timer;
    xSemaphoreGiveRecursive(timers_lock);
    return timer;
}

void os_timer_set(os_timer_t *timer, uint32_t us) { timer->us = us; }

// (re)start: a running timer starts over
void os_timer_start(os_timer_t *timer)
{
    cancel(timer);
    taskENTER_CRITICAL();
    timer->at = time_us_64() + (timer->us ? timer->us : 1);
    alarm_id_t id = alarm_pool_add_alarm_at(alarm_pool_get_default(), from_us_since_boot(timer->at), on_alarm,
                                            timer, false);
    timer->alarm = id > 0 ? id : 0;
    if (id <= 0) timer->due = true;   // already past (0), or no free alarm (<0): run it now
    taskEXIT_CRITICAL();
    if (id < 0) os_log(LOG_LEVEL_ERROR, "os_timer: no free alarm (PICO_TIME_DEFAULT_ALARM_POOL_MAX_TIMERS)\n");
    if (id <= 0) xTaskNotifyGive(timer_task);
}

void os_timer_stop(os_timer_t *timer) { cancel(timer); }

void os_timer_destroy(os_timer_t *timer)
{
    cancel(timer);
    xSemaphoreTakeRecursive(timers_lock, portMAX_DELAY);
    for (os_timer_t **p = &timers; *p; p = &(*p)->next) {
        if (*p == timer) {
            *p = timer->next;
            break;
        }
    }
    xSemaphoreGiveRecursive(timers_lock);
    vPortFree(timer);
}

// p-net's messages end in "\n" themselves; the prefix reads like the rest of the log.
// Queued, not printed: see log_rp2.c
void os_log(uint8_t type, const char *fmt, ...)
{
    static const char letter[] = "DIWEF";
    char line[256];
    int n = snprintf(line, sizeof(line), "%c (%lu) pnet: ", letter[LOG_LEVEL_GET(type) % 5],
                     (unsigned long)to_ms_since_boot(get_absolute_time()));
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    size_t len = m < 0 ? (size_t)n : (size_t)n + (size_t)m;
    if (len >= sizeof(line)) {
        len = sizeof(line) - 1;
        line[len - 1] = '\n';
    }
    pnal_rp2_log_write(line, len);
}
