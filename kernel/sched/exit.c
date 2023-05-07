#include <kernel/vm.h>
#include <kernel/sys.h>
#include <kernel/process.h>

static inline void process_space_free(struct process* proc)
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

    log_debug(DEBUG_EXIT, "freeing pgd");
    page_free(proc->mm->pgd);
    log_debug(DEBUG_EXIT, "freeing kstack");
    page_free(kernel_stack_end);
    delete(proc->mm);
}

void process_delete(struct process* proc)
{
    log_debug(DEBUG_EXIT, "");
    list_del(&proc->siblings);
    list_del(&proc->children);
    list_del(&proc->processes);
    process_space_free(proc);
    arch_process_free(proc);
    process_fs_exit(proc);
    process_files_exit(proc);
    process_signals_exit(proc);
    process_bin_exit(proc);
    delete(proc);
}

int sys_waitpid(int pid, int* status, int wait_flags)
{
    struct process* proc;
    flags_t flags;

    log_debug(DEBUG_EXIT, "%u called for %u; flags = %d", process_current->pid, pid, wait_flags);

    if (process_find(pid, &proc))
    {
        return -ESRCH;
    }

    log_debug(DEBUG_EXIT, "proc state = %c", process_state_char(proc->stat));

    if (process_is_zombie(proc))
    {
        log_debug(DEBUG_EXIT, "proc %d is zombie", proc->pid);
        goto dont_wait;
    }
    else if (process_is_stopped(proc) && wait_flags == WUNTRACED)
    {
        log_debug(DEBUG_EXIT, "proc %d is already stopped", proc->pid);
        goto dont_wait;
    }

    WAIT_QUEUE_DECLARE(temp, process_current);

    irq_save(flags);
    temp.flags = wait_flags;
    wait_queue_push(&temp, &proc->wait_child);
    process_wait2(process_current, flags);

dont_wait:
    if (proc->stat == PROCESS_STOPPED)
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

    return 0;
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
