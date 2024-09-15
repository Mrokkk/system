#include <kernel/debug.h>
#include <kernel/procfs.h>
#include <kernel/signal.h>
#include <kernel/process.h>

PROCESS_DECLARE(init_process);

int process_find(int pid, process_t** p)
{
    process_t* proc;

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

int process_find_free_fd_at(process_t* p, int at, int* fd)
{
    int i;
    file_t* dummy = NULL;

    *fd = 0;

    if (unlikely(at < 0))
    {
        return 1;
    }

    // Find first free file descriptor
    for (i = at; i < PROCESS_FILES; i++)
    {
        if (process_fd_get(p, i, &dummy)) break;
    }

    if (dummy)
    {
        return 1;
    }

    *fd = i;

    log_debug(DEBUG_PROCESS, "found %d, %x", *fd, p->files + i);

    return 0;
}

int process_find_free_fd(process_t* p, int* fd)
{
    return process_find_free_fd_at(p, 0, fd);
}

int sys_getpid(void)
{
    return process_current->pid;
}

int sys_getppid(void)
{
    return process_current->ppid;
}

int sys_getuid(void)
{
    return 0;
}

int sys_geteuid(void)
{
    return 0;
}

int sys_getgid(void)
{
    return 0;
}

int sys_getegid(void)
{
    return 0;
}

int sys_setuid(uid_t)
{
    return 0;
}

int sys_setgid(gid_t)
{
    return 0;
}

pid_t sys_setpgrp(void)
{
    return -1;
}

pid_t sys_getpgrp(void)
{
    return process_current->pgid;
}

int sys_setsid(void)
{
    if (process_current->sid == process_current->pid)
    {
        return -EPERM;
    }
    process_current->sid = process_current->pid;
    return 0;
}

int processes_init()
{
    init_process.kernel_stack = ptr(&init_process_stack[INIT_PROCESS_STACK_SIZE]);
    init_process.mm->pgd = init_pgd_get();
    init_process.mm->vm_areas = NULL;
    mutex_init(&init_process.mm->lock);
    list_add_tail(&init_process.running, &running);
    arch_process_init();
    return 0;
}
