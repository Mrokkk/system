#include <kernel/process.h>
#include <kernel/sys.h>
#include <arch/process.h>

static int process_state_change(struct process *proc, int stat);

/* Stack for init process */
unsigned long init_stack[INIT_PROCESS_STACK_SIZE] = { STACK_MAGIC, };

/* Init process itself */
struct process init_process = INIT_PROCESS;

DECLARE_LIST(process_list);
DECLARE_LIST(running);
DECLARE_LIST(waiting);
DECLARE_LIST(stopped);

static pid_t last_pid = INIT_PROCESS_PID;

/*===========================================================================*
 *                               process_find                                *
 *===========================================================================*/
KERNEL_INIT(processes_init, INIT_PRIORITY_HI)() {

    int i;

    list_add_back(&init_process.processes, &process_list);
    list_add_back(&init_process.queue, &running);
    for (i=0; i<PROCESS_FILES; i++)
        process_current->files[i] = 0;

    return 0;

}

/*===========================================================================*
 *                               process_find                                *
 *===========================================================================*/
struct process *process_find(int pid) {

    struct process *proc;

    list_for_each_entry(proc, &process_list, processes) {
        if (proc->pid == pid)
            return proc;
    }

    return 0;

}

/*===========================================================================*
 *                               find_free_pid                               *
 *===========================================================================*/
pid_t find_free_pid() {
    return ++last_pid;
}

/*===========================================================================*
 *                           process_state_change                            *
 *===========================================================================*/
static int process_state_change(struct process *proc, int stat) {

    //struct wait_queue *process_on_list = 0;
    unsigned int flags;

    if (!proc) return -ESRCH;
    if (proc->stat == stat) printk("cannot change\n");

    save_flags(flags);
    cli();

    if (proc->stat != NO_PROCESS)
        list_del(&proc->queue);

    switch (stat) {
        case PROCESS_RUNNING:
            list_add_back(&proc->queue, &running);
            break;
        case PROCESS_WAITING:
            list_add_back(&proc->queue, &waiting);
            break;
        case PROCESS_STOPPED:
            list_add_back(&proc->queue, &stopped);
            break;
        case NO_PROCESS:
            /* Nothing */
            break;
    }

    proc->stat = stat;

    restore_flags(flags);

    return 0;

}

/*===========================================================================*
 *                              process_exit                                 *
 *===========================================================================*/
int process_exit(struct process *proc) {

    int stat;

    if (proc->pid == INIT_PROCESS_PID) { /* TODO: Shutdown system */
        printk("Init returned %d: shutdown now...\n", proc->errno);
        while (1);
    }

    if ((stat = process_state_change(proc, NO_PROCESS))) {
        printk("cannot change state of process %d :: %d\n", proc->pid, stat);
        return 1;
    }

    while (1);

    return 0;

}

/*===========================================================================*
 *                               process_stop                                *
 *===========================================================================*/
int process_stop(struct process *proc) {
    return process_state_change(proc, PROCESS_STOPPED);
}

/*===========================================================================*
 *                               process_wake                                *
 *===========================================================================*/
int process_wake(struct process *proc) {
    return process_state_change(proc, PROCESS_RUNNING);
}

/*===========================================================================*
 *                              kthread_create                               *
 *===========================================================================*/
int kthread_create(int (*start)(), const char *name) {

    struct pt_regs regs;
    int pid;

    arch_kthread_regs_init(&regs, (unsigned int)start);

    pid = sys_fork(regs);
    if (pid < 0) goto end;

    sprintf(process_find(pid)->name, "[%s]", name);

    end:
         return pid;

}

