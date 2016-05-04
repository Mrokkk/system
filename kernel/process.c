#include <kernel/process.h>
#include <kernel/sys.h>

PROCESS_DECLARE(init_process);
LIST_DECLARE(running);

static pid_t last_pid;

unsigned int total_forks;

/*===========================================================================*
 *                              process_forked                               *
 *===========================================================================*/
static inline void process_forked(struct process *proc) {
    proc->forks++;
    total_forks++;
}

/*===========================================================================*
 *                            process_space_free                             *
 *===========================================================================*/
static inline void process_space_free(struct process *proc) {
    kfree(proc->mm.start);
}

/*===========================================================================*
 *                           process_signals_free                            *
 *===========================================================================*/
static inline void process_signals_free(struct process *proc) {
    if (proc->signals)
        kfree(proc->signals);
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
 *                               processes_init                              *
 *===========================================================================*/
int processes_init() {

    /*
     * Just add init_process to the running queue
     */
    list_add_tail(&init_process.running, &running);

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
 *                            process_wake_waiting                           *
 *===========================================================================*/
void process_wake_waiting(struct process *proc) {

    struct process *temp;

    if (list_empty(&proc->wait_queue)) return;

    list_for_each_entry(temp, &proc->wait_queue, wait_queue)
        process_wake(temp);

}

/*===========================================================================*
 *                            process_space_setup                            *
 *===========================================================================*/
static int process_space_setup(struct process *proc) {

    char *start, *end;

    /* Set up process space */
    if (!(end = start = (char *)kmalloc(PROCESS_SPACE)))
        return -ENOMEM;

    end += PROCESS_SPACE;
    proc->mm.start = start;
    proc->mm.end = end;

    return 0;

}

/*===========================================================================*
 *                              process_delete                               *
 *===========================================================================*/
void process_delete(struct process *proc) {

    list_del(&proc->siblings);
    list_del(&proc->children);
    list_del(&proc->processes);
    process_space_free(proc);
    arch_process_free(proc);
    process_signals_free(proc);
    kfree(proc);

}

/*===========================================================================*
 *                            process_struct_init                            *
 *===========================================================================*/
static void process_struct_init(struct process *proc) {

    proc->pid = find_free_pid();
    proc->exit_code = 0;
    proc->signals = 0;
    proc->forks = 0;
    proc->context_switches = 0;
    proc->stat = PROCESS_ZOMBIE;
    *proc->name = 0;
    list_init(&proc->wait_queue);
    list_init(&proc->children);
    list_init(&proc->siblings);

}

/*===========================================================================*
 *                               process_create                              *
 *===========================================================================*/
static struct process *process_create(int type) {

    struct process *new_process;

    (void)type;

    /* Allocate space for process structure */
    if (!(new_process = (struct process *)kmalloc(sizeof(struct process))))
        goto cannot_create_process;

    if (process_space_setup(new_process))
        goto cannot_allocate;

    process_struct_init(new_process);
    new_process->type = type;

    return new_process;

cannot_allocate:
    process_delete(new_process);
cannot_create_process:
    return 0;

}

/*===========================================================================*
 *                                process_copy                               *
 *===========================================================================*/
static void process_copy(struct process *dest, struct process *src,
        struct pt_regs *regs, int clone_flags) {

    strcpy(dest->name, src->name);
    if (clone_flags & CLONE_FILES)
        memcpy(dest->files, src->files, sizeof(struct file *) * PROCESS_FILES);
    else
        memset(dest->files, 0, sizeof(struct file *) * PROCESS_FILES);

    arch_process_copy(dest, src, regs);

}

/*===========================================================================*
 *                          process_parent_child_link                        *
 *===========================================================================*/
static void process_parent_child_link(struct process *parent,
        struct process *child) {

    child->parent = parent;
    child->ppid = parent->pid;
    list_add(&child->siblings, &parent->children);

}

/*===========================================================================*
 *                               process_clone                               *
 *===========================================================================*/
int process_clone(struct process *parent, struct pt_regs *regs,
        int clone_flags) {

    struct process *child;
    int flags;

    child = process_create(process_type(parent));
    process_parent_child_link(parent, child);
    process_copy(child, parent, regs, clone_flags);
    list_add_tail(&child->processes, &init_process.processes);
    process_forked(parent);

    save_flags(flags);
    cli();

    process_wake(child);

    restore_flags(flags);

    return child->pid;

}

/*===========================================================================*
 *                               kernel_process                              *
 *===========================================================================*/
int kernel_process(int (*start)(), char *name) {

    int pid;

    if ((pid = fork()) == 0)
        exit(start());
    else if (pid < 0) return pid;

    strcpy(process_find(pid)->name, name);

    return pid;

}

