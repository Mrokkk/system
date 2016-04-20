#include <kernel/process.h>
#include <arch/process.h>
#include <arch/io.h>

/*===========================================================================*
 *                            process_check_stat                             *
 *===========================================================================*/
__noinline int process_check_stat(int pid, char stat) {

    if (process_find(pid)->stat != stat)
        return 0;

    return 1;

}

/*===========================================================================*
 *                               sys_waitpid                                 *
 *===========================================================================*/
int sys_waitpid(int pid, int *status, int opt) {

    struct process *proc = process_find(pid);
    char last_stat;
    unsigned int flags;

    (void)opt;

    if (!proc) return -ESRCH;
    last_stat = proc->stat;

    while (process_check_stat(pid, last_stat));

    put_to_user(status, &(proc->errno));

    save_flags(flags);
    cli();

    if (proc->stat == NO_PROCESS) {
        list_del(&proc->processes);
        arch_process_free(proc);
        kfree(proc->mm.start);
        kfree(proc->name);
        if (proc->signals)
            kfree(proc->signals);
        kfree(proc);
    }

    restore_flags(flags);

    return 0;

}
