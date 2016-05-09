#include <kernel/process.h>
#include <arch/io.h>

/*===========================================================================*
 *                               sys_waitpid                                 *
 *===========================================================================*/
int sys_waitpid(int pid, int *status, int opt) {

    struct process *proc;
    unsigned int flags;

    (void)opt;

    if (process_find(pid, &proc)) return -ESRCH;
    if (proc->stat != PROCESS_RUNNING) goto delete_process;

    list_add_tail(&process_current->wait_queue, &proc->wait_queue);
    process_wait(process_current);

    put_user_long(status, &(proc->exit_code));

delete_process:
    save_flags(flags);
    cli();

    if (process_is_zombie(proc))
        process_delete(proc);

    restore_flags(flags);

    return 0;

}
