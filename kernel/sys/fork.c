#include <kernel/process.h>
#include <kernel/test.h>

unsigned int total_forks;

/*
 * Fork implementation is now more flexible and handles also
 * forking in kernel process
 */

/*===========================================================================*
 *                                  sys_fork                                 *
 *===========================================================================*/
int sys_fork(struct pt_regs regs) {

    /*
     * Not quite compatible with POSIX, but does its work
     */

    struct process *new;
    int errno = -ENOMEM;

    if (!(new = process_create(USER_PROCESS))) goto error;

    process_copy(new, process_current, CLONE_FILES, &regs);

    total_forks++;

    if ((errno = process_wake(new))) goto error;

    return new->pid;

error:
    return errno;

}
