#include <kernel/process.h>

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

    return process_clone(process_current, &regs, CLONE_FILES);

}
