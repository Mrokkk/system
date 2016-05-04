#include <kernel/process.h>

struct process *process_current = &init_process;

unsigned int context_switches;

/*===========================================================================*
 *                                scheduler                                  *
 *===========================================================================*/
void scheduler() {

    /* Simple RR scheduler */

    struct process *last = process_current;

    if (list_empty(&running)) {
        /* If there are no processes on the running queue,
         * run init process, which is the idle process
         */
        process_current = &init_process;
        goto end;
    }

    if (process_current->stat != PROCESS_RUNNING) {
        process_current = list_entry(running.next, struct process, running);
    } else {
        struct list_head *temp = process_current->running.next;
        /* I don't like it, but it works... */
        if (temp == &running) temp = temp->next;
        process_current = list_entry(temp, struct process, running);
    }

end:

    if (last == process_current) return;

    context_switches++;
    process_current->context_switches++;

    /* There was sti(), which was causing bug (why?) I
     * couldn't fix for hours. But what is interesting,
     * it was present only on qemu 2.5.1. */
    process_switch(last, process_current);

}

