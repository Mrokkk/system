#include <kernel/process.h>
#include <arch/process.h>
#include <kernel/test.h>

unsigned int total_forks;

/*===========================================================================*
 *                                  sys_fork                                 *
 *===========================================================================*/
int sys_fork(struct pt_regs regs) {

    /*
     * Not quite compatible with POSIX, but does its work
     */

    struct process *new_process = 0;
    int child_pid;
    char error = -ENOMEM;
    void *start, *end;                      /* Alloc process space */
    unsigned int *new_stack = (unsigned int *)kmalloc(PROCESS_SPACE);
    unsigned int flags;

    save_flags(flags);
    cli();

    if (!new_stack)
        goto fork_error0;

    /* Allocate memory for process structure */
    new_process = kmalloc(sizeof(struct process));
    if (unlikely(!new_process))
        goto fork_error1;

    /* Set up process space */
    start = (void *)new_stack;
    new_stack = (void *)((unsigned int)new_stack + PROCESS_SPACE);
    end = (void *)new_stack;

    list_add_back(&new_process->processes, &process_list);
    new_process->parent = process_current;
    new_process->ppid = process_current->pid;
    new_process->mm.start = start;
    new_process->mm.end = end;
    new_process->errno = 0;
    new_process->signals = 0;
    new_process->wait_queue = 0;
    new_process->forks = 0;
    new_process->context_switches = 0;
    new_process->kernel = 0;
    new_process->stat = NO_PROCESS;

    /* Set up name */
    new_process->name = kmalloc(16);
    if (!new_process->name)
        goto fork_error2;
    strcpy(new_process->name, process_current->name);

    /* Copy open file descriptors */
    memcpy(new_process->files, process_current->files, PROCESS_FILES);

    /* Call arch-dependent function copying and initializing context */
    if ((error = process_copy(new_process, process_current, new_stack, &regs)))
        goto fork_error3;

    /* Set PID to process and insert      *
     * pointer to new process into others */
    child_pid = find_free_pid();
    new_process->pid = child_pid;
    process_current->y_child = new_process;

    /* Wake process up by inserting it to ready list */
    if ((error = process_wake(new_process))) {
        goto fork_error4;
    }

    total_forks++;
    process_current->forks++;

    restore_flags(flags);

    return child_pid;

    /* On error - cleanup */
    fork_error4: process_current->y_child = 0;
    fork_error3: arch_process_free(new_process);
                 kfree(new_process->name);
    fork_error2: kfree(new_process);
    fork_error1: kfree(new_stack);
    fork_error0: restore_flags(flags);
                 return -error;

}
