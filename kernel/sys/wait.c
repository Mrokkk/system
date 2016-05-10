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

    put_user_long(proc->exit_code, status);

delete_process:

    irq_save(flags);

    if (process_is_zombie(proc))
        process_delete(proc);

    irq_restore(flags);

    return 0;

}
