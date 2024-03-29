#pragma once

#include <kernel/wait.h>

struct semaphore
{
    volatile int count;
    volatile int waiting;
    wait_queue_head_t queue;
};

typedef struct semaphore semaphore_t;

#define SEMAPHORE_INIT(sem, count) \
    { count, 0, WAIT_QUEUE_HEAD_INIT(sem.queue) }

#define SEMAPHORE_DECLARE(sem, count) \
    semaphore_t sem = SEMAPHORE_INIT(sem, count)

static inline void semaphore_init(semaphore_t* semaphore, int count)
{
    semaphore->count = count;
    semaphore->waiting = 0;
    wait_queue_head_init(&semaphore->queue);
}

#include <arch/semaphore.h>
