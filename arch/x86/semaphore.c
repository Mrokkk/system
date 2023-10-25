#include <arch/system.h>

#include <kernel/process.h>
#include <kernel/semaphore.h>

void __down_failed(semaphore_t* sem)
{
    WAIT_QUEUE_DECLARE(q, process_current);

    sem->waiting++;

#pragma GCC diagnostic push
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wdangling-pointer="
#endif
    while (process_wait(&sem->queue, &q));
#pragma GCC diagnostic pop
}

void __up(semaphore_t* sem)
{
    struct process* proc;

    proc = wait_queue_front(&sem->queue);
    if (!proc)
    {
        return;
    }

    sem->waiting--;
    process_wake(proc);
}
