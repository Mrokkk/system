#include <kernel/vm.h>
#include <kernel/timer.h>
#include <kernel/process.h>
#include <kernel/api/unistd.h>

static inline void process_space_free(process_t* proc)
{
    pgd_t* pgd = proc->mm->pgd;
    uint32_t* kernel_stack_end = ptr(addr(proc->mm->kernel_stack) - PAGE_SIZE);

    log_debug(DEBUG_EXIT, "pid %u", proc->pid);

    if (unlikely(*kernel_stack_end != STACK_MAGIC))
    {
        panic("kernel stack overflow; stack @ %x, expected: %x; got: %x",
            kernel_stack_end,
            STACK_MAGIC,
            *kernel_stack_end);
    }

    vm_free(proc->mm->vm_areas, pgd);

    page_free(proc->mm->pgd);
    page_free(kernel_stack_end);
    delete(proc->mm);
}

static void process_delete(process_t* proc)
{
    log_debug(DEBUG_EXIT, "");
    list_del(&proc->siblings);
    list_del(&proc->children);
    list_del(&proc->processes);

    // FIXME: perhaps those should be called directly in process_exit as well
    {
        process_space_free(proc);
        arch_process_free(proc);
        process_fs_exit(proc);
        process_signals_exit(proc);
    }

    delete(proc);
}

int sys_waitpid(int pid, int* status, int wait_flags)
{
    process_t* proc;

    log_debug(DEBUG_EXIT, "called for %d; flags = %d", pid, wait_flags);

    if (pid == -1)
    {
        while (!list_empty(&process_current->children))
        {
            list_for_each_entry(proc, &process_current->children, siblings)
            {
                if (process_is_zombie(proc))
                {
                    goto dont_wait;
                }
                else if (process_is_stopped(proc) && wait_flags & WUNTRACED)
                {
                    goto dont_wait;
                }
            }

            if (wait_flags & WNOHANG)
            {
                return 0;
            }

            WAIT_QUEUE_DECLARE(temp, process_current);

            temp.flags = wait_flags;
            process_wait(&process_current->wait_child, &temp);

            log_debug(DEBUG_EXIT, "woken %u", process_current->pid);
        }
        return 0;
    }
    else if (process_find(pid, &proc))
    {
        return -ESRCH;
    }

    log_debug(DEBUG_EXIT, "proc state = %c", process_state_char(proc->stat));

    if (process_is_zombie(proc))
    {
        goto dont_wait;
    }
    else if (process_is_stopped(proc) && wait_flags & WUNTRACED)
    {
        goto dont_wait;
    }

    WAIT_QUEUE_DECLARE(temp, process_current);

    while (process_is_running(proc) || proc->stat == PROCESS_WAITING)
    {
        temp.flags = wait_flags;
        process_wait(&process_current->wait_child, &temp);

        log_debug(DEBUG_EXIT, "woken %u; proc %u state = %c", process_current->pid, proc->pid, process_state_char(proc->stat));
    }

dont_wait:
    if (process_is_stopped(proc))
    {
        *status = WSTOPPED;
    }
    else
    {
        *status = proc->exit_code;
    }

    if (process_is_zombie(proc))
    {
        process_delete(proc);
    }

    return proc->pid;
}

void process_wake_waiting(process_t* proc)
{
    process_t* parent = proc->parent;

    if (unlikely(!parent))
    {
        return;
    }

    if (wait_queue_empty(&parent->wait_child))
    {
        return;
    }

    wait_queue_t* q = list_front(&parent->wait_child.queue, wait_queue_t, processes);
    if (q->flags == WUNTRACED || proc->stat == PROCESS_ZOMBIE)
    {
        wait_queue_pop(&parent->wait_child);
        log_debug(DEBUG_EXIT, "waking %u", parent->pid);
        process_wake(parent);
        return;
    }

    log_debug(DEBUG_EXIT, "not waking %u", parent->pid);
}

void process_exit(process_t* p)
{
    scoped_irq_lock();

    log_debug(DEBUG_EXIT, "%u exiting", p->pid);

    list_del(&p->running);
    process_files_exit(p);
    p->stat = PROCESS_ZOMBIE;
    process_wake_waiting(p);
    p->need_resched = true;
    process_ktimers_exit(p);
}

int sys_exit(int return_value)
{
    log_debug(DEBUG_EXIT, "process pid=%u, %S exited with %d",
        process_current->pid,
        process_current->name,
        return_value);

    process_current->exit_code = EXITCODE(return_value, 0);
    strcpy(process_current->name, "<defunct>");
    process_exit(process_current);
    return 0;
}
