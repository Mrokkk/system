#include <arch/system.h>

#include <kernel/process.h>
#include <kernel/semaphore.h>

void __down_failed(semaphore_t* sem)
{
    WAIT_QUEUE_DECLARE(__sem, process_current);

    flags_t flags;
    irq_save(flags);

    wait_queue_push(&__sem, &sem->queue);
    sem->waiting++;

    process_wait2(process_current, flags);
}

void __up(semaphore_t* sem)
{
    struct process* proc;

    proc = wait_queue_pop(&sem->queue);
    if (!proc)
    {
        return;
    }

    sem->waiting--;
    process_wake(proc);
}
