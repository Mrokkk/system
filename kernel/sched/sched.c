#include <kernel/process.h>
#include <arch/context_switch.h>

struct process* process_current = &init_process;
unsigned int context_switches;
unsigned* need_resched;

// Simple RR scheduler
void scheduler()
{
    struct process* last = process_current;

    if (list_empty(&running))
    {
        // If there are no processes on the running queue,
        // run init process, which is the idle process
        process_current = &init_process;
        goto end;
    }

    if (!process_is_running(process_current))
    {
        process_current = list_entry(running.next, struct process, running);
    }
    else
    {
        list_head_t* temp = process_current->running.next;
        if (temp == &running)
        {
            temp = temp->next;
        }
        process_current = list_entry(temp, struct process, running);
    }

#if PARANOIA_SCHED
    if (unlikely(process_current->stat != PROCESS_RUNNING))
    {
        panic("bug: process %u:%x stat is %u; expected %u; addr=%x",
            process_current->pid,
            process_current,
            process_current->stat,
            PROCESS_RUNNING,
            &process_current->stat);
    }
#endif

end:
    if (last == process_current)
    {
        return;
    }

    context_switches++;
    last->context_switches++;
    need_resched = &process_current->need_resched;

    process_switch(last, process_current);
}
