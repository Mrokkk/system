#include <kernel/trace.h>
#include <kernel/procfs.h>
#include <kernel/process.h>

PROCESS_DECLARE(init_process);
LIST_DECLARE(running);

int process_find(int pid, struct process** p)
{
    struct process* proc;

    for_each_process(proc)
    {
        if (proc->pid == pid)
        {
            *p = proc;
            return 0;
        }
    }

    *p = NULL;
    return 1;
}

void process_wake_waiting(struct process* proc)
{
    struct process* parent = proc->parent;

    if (wait_queue_empty(&proc->wait_child))
    {
        return;
    }

    wait_queue_t* q = list_front(&proc->wait_child.queue, struct wait_queue, processes);
    if (q->flags == WUNTRACED || proc->stat == PROCESS_ZOMBIE)
    {
        wait_queue_pop(&proc->wait_child);
        log_debug(DEBUG_PROCESS, "waking %u", parent->pid);
        process_wake(parent);
        return;
    }

    log_debug(DEBUG_PROCESS, "not waking %u", parent->pid);
}

int process_find_free_fd(struct process* proc, int* fd)
{
    struct file* dummy;
    int i;

    *fd = 0;

    // Find first free file descriptor
    for (i = 0; i < PROCESS_FILES; i++)
    {
        if (process_fd_get(proc, i, &dummy)) break;
    }

    if (dummy)
    {
        return 1;
    }

    *fd = i;

    log_debug(DEBUG_PROCESS, "found %d, %x", *fd, proc->files);

    return 0;
}

int sys_getpid()
{
    return process_current->pid;
}

int sys_getppid()
{
    return process_current->ppid;
}

char* mm_print(const struct mm* mm, char* str)
{
    str += sprintf(str,
        "mm{\n"
        "\t\taddr = %x,\n"
        "\t\tstack_start =  %08x,\n"
        "\t\tstack_end =    %08x,\n"
        "\t\targs_start =   %08x,\n"
        "\t\targs_end =     %08x,\n"
        "\t\tenv_start =    %08x,\n"
        "\t\tenv_end =      %08x,\n"
        "\t\tbrk =          %08x,\n"
        "\t\tkernel_stack = %08x,\n"
        "\t\tpgd =          %08x,\n"
        "\t}", mm, mm->stack_start, mm->stack_end, mm->args_start, mm->args_end, mm->env_start, mm->env_end,
        mm->brk, mm->kernel_stack, mm->pgd);
    return str;
}

char* fs_print(const void* data, char* str)
{
    const struct fs* fs = data;
    str += sprintf(str,
        "fs{\n"
        "\t\taddr = %x,\n"
        "\t\tcount = %x,\n"
        "\t\tcwd = %O\n"
        "\t}", fs, fs->count, fs->cwd);
    return str;
}

char* process_print(const struct process* p, char* str)
{
    str += sprintf(str,
        "process{\n"
        "\taddr = %x,\n"
        "\tname = %S\n"
        "\tpid = %u\n"
        "\tppid = %u\n"
        "\tstat = %c,\n"
        "\ttype = %u,\n"
        "\tcontext_switches = %u,\n"
        "\tforks = %u,\n"
        "\tmm = ",
        p, p->name, p->pid, p->ppid, process_state_char(p->stat), p->type, p->context_switches, p->forks);

    str = mm_print(p->mm, str);
    str += sprintf(str, ",\n\tfs = ");
    str = fs_print(p->fs, str);
    str += sprintf(str, "\n}");

    return str;
}

int processes_init()
{
    init_process.mm->pgd = init_pgd_get();
    init_process.mm->kernel_stack = ptr(&init_process_stack[INIT_PROCESS_STACK_SIZE]);
    init_process.mm->vm_areas = NULL;
    need_resched = &init_process.need_resched;
    mutex_init(&init_process.mm->lock);
    list_add_tail(&init_process.running, &running);
    return 0;
}
