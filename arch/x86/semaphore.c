#include <kernel/semaphore.h>
#include <arch/system.h>

void __down_failed(semaphore_t *sem) {

    WAIT_QUEUE_DECLARE(__sem, process_current);

    wait_queue_push(&__sem, &sem->queue);
    sem->waiting++;

    process_wait(process_current);

}

void __up(semaphore_t *sem) {

    struct process *proc;

    proc = wait_queue_pop(&sem->queue);
    if (!proc) return;
    sem->waiting--;
    process_wake(proc);

}
