#include <kernel/process.h>
#include <arch/context_switch.h>

static_assert(NEED_RESCHED_SIGNAL_OFFSET == offsetof(process_t, _need_resched));

process_t* process_current = &init_process;
LIST_DECLARE(running);
unsigned int context_switches;

// Simple RR scheduler
void scheduler(void)
{
    process_t* last = process_current;

    if (list_empty(&running))
    {
        // If there are no processes on the running queue,
        // run init process, which is the idle process
        process_current = &init_process;
        goto end;
    }

    if (!process_is_running(process_current))
    {
        process_current = list_entry(running.next, process_t, running);
    }
    else
    {
        list_head_t* temp = process_current->running.next;
        if (temp == &running)
        {
            temp = temp->next;
        }
        process_current = list_entry(temp, process_t, running);
    }

#if PARANOIA_SCHED
    if (unlikely(process_current->stat != PROCESS_RUNNING))
    {
        panic("bug: process %u:%x stat is %u; expected %u",
            process_current->pid,
            process_current,
            process_current->stat,
            PROCESS_RUNNING);
    }
#endif

end:
    if (last == process_current)
    {
        return;
    }

    context_switches++;
    last->context_switches++;

    process_switch(last, process_current);
}
