/*
 * osal_sys.h - the OSAL types on the RP2350. A thread, mutex, semaphore, mailbox and event are the
 * FreeRTOS objects themselves (os_mutex_t * is a SemaphoreHandle_t, and so on); a timer is a
 * hardware alarm of the pico_time default alarm pool, for microsecond periods instead of ticks.
 */
#ifndef OSAL_SYS_H
#define OSAL_SYS_H

#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "event_groups.h"
#include "pico/time.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OS_THREAD
#define OS_MUTEX
#define OS_SEM
#define OS_EVENT
#define OS_MBOX
#define OS_TIMER

typedef struct tskTaskControlBlock os_thread_t;
typedef struct QueueDefinition os_mutex_t;
typedef struct QueueDefinition os_sem_t;
typedef struct QueueDefinition os_mbox_t;
typedef struct EventGroupDef_t os_event_t;

typedef struct os_timer {
    alarm_id_t alarm;   // 0: not running
    uint64_t at;        // when the running alarm is due (us since boot)
    void (*fn)(struct os_timer *timer, void *arg);
    void *arg;
    uint32_t us;
    bool oneshot;
    volatile bool due;   // set by the alarm, cleared when the callback has run
    struct os_timer *next;
} os_timer_t;

#ifdef __cplusplus
}
#endif

#endif /* OSAL_SYS_H */
