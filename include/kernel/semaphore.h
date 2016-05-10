#ifndef INCLUDE_KERNEL_SEMAPHORE_H_
#define INCLUDE_KERNEL_SEMAPHORE_H_

#include <kernel/wait.h>

typedef struct semaphore {
    volatile int count;
    volatile int waiting;
    struct wait_queue_head queue;
} semaphore_t;

#define SEMAPHORE_INIT(sem, count) \
    { count, 0, WAIT_QUEUE_HEAD_INIT(sem.queue) }

#define SEMAPHORE_DECLARE(sem, count) \
    semaphore_t sem = SEMAPHORE_INIT(sem, count)

#include <arch/semaphore.h>

#endif /* INCLUDE_KERNEL_SEMAPHORE_H_ */
