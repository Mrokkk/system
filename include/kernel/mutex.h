#pragma once

#include <kernel/semaphore.h>

typedef semaphore_t mutex_t;

#define MUTEX_INIT(m) \
    { 1, 0, WAIT_QUEUE_HEAD_INIT((m).queue) }

#define MUTEX_DECLARE(m) \
    mutex_t m = MUTEX_INIT(m)

#define mutex_init(m) semaphore_init(m, 1)

static inline void mutex_lock(mutex_t* m)
{
    semaphore_down((semaphore_t*)m);
}

static inline void mutex_unlock(mutex_t* m)
{
    semaphore_up((semaphore_t*)m);
}

#include <arch/mutex.h>
