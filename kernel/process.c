#include <kernel/process.h>
#include <kernel/sys.h>
#include <arch/process.h>

static int process_state_change(struct process *proc, int stat);

/* Stacks for init process */
unsigned long init_stack[INIT_PROCESS_STACK_SIZE] = { STACK_MAGIC, };
unsigned long init_kernel_stack[INIT_PROCESS_STACK_SIZE] = { STACK_MAGIC, };

/* Init process itself */
struct process init_process = PROCESS_INIT(init_process);

DECLARE_LIST(running);
DECLARE_LIST(waiting);
DECLARE_LIST(stopped);

static pid_t last_pid = INIT_PROCESS_PID;

/*===========================================================================*
 *                               processes_init                              *
 *===========================================================================*/
int processes_init() {

    /*
     * Just add init_process to running queue
     */
    list_add_back(&init_process.queue, &running);

    return 0;

}

/*===========================================================================*
 *                               process_find                                *
 *===========================================================================*/
struct process *process_find(int pid) {

    struct process *proc;

    list_for_each_entry(proc, &init_process.processes, processes) {
        if (proc->pid == pid)
            return proc;
    }

    return 0;

}

/*===========================================================================*
 *                               find_free_pid                               *
 *===========================================================================*/
pid_t find_free_pid() {
    /*
     * Just incrementing PID. Maybe it should be more
     * complicated (some hash table maybe?)
     */
    return ++last_pid;
}

/*===========================================================================*
 *                           process_state_change                            *
 *===========================================================================*/
static int process_state_change(struct process *proc, int stat) {

    unsigned int flags;

    if (!proc) return -ESRCH;
    if (proc->stat == stat) printk("cannot change\n");

    save_flags(flags);
    cli();

    /* Remove process form its current queue... */
    if (proc->stat != NO_PROCESS)
        list_del(&proc->queue);

    /* ...and add it to the relevant one */
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
 *                                process_wake                               *
 *===========================================================================*/
int process_wake(struct process *proc) {
    return process_state_change(proc, PROCESS_RUNNING);
}

/*===========================================================================*
 *                               process_create                              *
 *===========================================================================*/
struct process *process_create(int type) {

    struct process *new_process;
    char *start, *end;

    /* Allocate space for process structure */
    if (!(new_process = (struct process *)kmalloc(sizeof(struct process))))
        goto cannot_create_process;

    /* Set up process space */
    if (!(end = start = (char *)kmalloc(PROCESS_SPACE)))
        goto cannot_create_space;
    end += PROCESS_SPACE;

    new_process->kernel = (type == KERNEL_PROCESS);
    new_process->pid = find_free_pid();
    new_process->parent = process_current;
    new_process->ppid = process_current->pid;
    new_process->mm.start = start;
    new_process->mm.end = end;
    new_process->errno = 0;
    new_process->signals = 0;
    new_process->wait_queue = 0;
    new_process->forks = 0;
    new_process->context_switches = 0;
    new_process->stat = NO_PROCESS;
    new_process->name = (char *)kmalloc(16);

    strcpy(new_process->name, "new");

    arch_process_init(new_process);

    list_add_back(&new_process->processes, &init_process.processes);

    process_current->y_child = new_process;
    process_current->forks++;

    return new_process;

cannot_create_space:
    kfree(new_process);
cannot_create_process:
    return 0;

}

/*===========================================================================*
 *                                process_copy                               *
 *===========================================================================*/
void process_copy(struct process *dest, struct process *src, int clone_flags,
        struct pt_regs *regs) {

    unsigned int flags;

    save_flags(flags);
    cli();

    strcpy(dest->name, src->name);
    if (clone_flags & CLONE_FILES)
        memcpy(dest->files, src->files, sizeof(struct file *) * PROCESS_FILES);
    else
        memset(dest->files, 0, sizeof(struct file *) * PROCESS_FILES);

    arch_process_copy(dest, src, regs);

    restore_flags(flags);

}

/*===========================================================================*
 *                              kthread_create                               *
 *===========================================================================*/
int kprocess_create(int (*start)(), const char *name) {

     struct pt_regs regs;
     struct process *new;

     arch_kprocess_regs_init(&regs, (unsigned int)start);

     new = process_create(KERNEL_PROCESS);
     process_copy(new, process_current, CLONE_FILES, &regs);

     strcpy(new->name, name);

     process_wake(new);

     return new->pid;

}

