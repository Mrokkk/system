#ifndef INCLUDE_KERNEL_MUTEX_H_
#define INCLUDE_KERNEL_MUTEX_H_

#include <kernel/semaphore.h>

typedef semaphore_t mutex_t;

#define MUTEX_INIT(m) \
    { 1, 0, WAIT_QUEUE_HEAD_INIT(m.queue) }

#define MUTEX_DECLARE(m) \
    mutex_t m = MUTEX_INIT(m)

static inline void mutex_lock(mutex_t *m) {
    semaphore_down((semaphore_t *)m);
}

static inline void mutex_unlock(mutex_t *m) {
    semaphore_up((semaphore_t *)m);
}

#endif /* INCLUDE_KERNEL_MUTEX_H_ */
