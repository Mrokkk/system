#include <arch/system.h>

#include <kernel/process.h>
#include <kernel/semaphore.h>

void __down_failed(semaphore_t* sem)
{
    WAIT_QUEUE_DECLARE(q, process_current);

    sem->waiting++;

    while (process_wait(&sem->queue, &q));
}

void __up(semaphore_t* sem)
{
    process_t* proc;

    proc = wait_queue_front(&sem->queue);
    if (!proc)
    {
        return;
    }

    sem->waiting--;
    process_wake(proc);
}
