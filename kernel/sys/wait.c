#include <kernel/process.h>
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
    unsigned int flags;

    (void)opt;

    if (!proc) return -ESRCH;
    if (proc->stat != PROCESS_RUNNING) goto delete_process;

    /*
     * There was a bug!
     * Callee process was waiting for process
     * even if that process wasn't running
     */

    list_add_tail(&process_current->wait_queue, &proc->wait_queue);
    process_wait(process_current);

    put_user_long(status, &(proc->exit_code));

delete_process:
    save_flags(flags);
    cli();

    if (proc->stat == PROCESS_ZOMBIE)
        process_delete(proc);

    restore_flags(flags);

    return 0;

}
