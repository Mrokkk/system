#include <kernel/process.h>
#include <kernel/sys.h>

PROCESS_DECLARE(init_process);
LIST_DECLARE(running);

static pid_t last_pid;

unsigned int total_forks;

/*===========================================================================*
 *                               find_free_pid                               *
 *===========================================================================*/
static inline pid_t find_free_pid() {
    /*
     * Just incrementing PID. Maybe it should be more
     * complicated (some hash table maybe?)
     */
    return ++last_pid;
}

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
 *                            process_space_setup                            *
 *===========================================================================*/
static inline int process_space_setup(struct process *proc) {

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
 *                            process_struct_init                            *
 *===========================================================================*/
static inline void process_struct_init(struct process *proc) {

    proc->pid = find_free_pid();
    proc->exit_code = 0;
    proc->exit_state = 0;
    proc->forks = 0;
    proc->context_switches = 0;
    proc->stat = PROCESS_ZOMBIE;
    *proc->name = 0;
    list_init(&proc->wait_queue);
    list_init(&proc->children);
    list_init(&proc->siblings);

}

/*===========================================================================*
 *                          process_parent_child_link                        *
 *===========================================================================*/
static inline void process_parent_child_link(struct process *parent,
        struct process *child) {

    child->parent = parent;
    child->ppid = parent->pid;
    list_add(&child->siblings, &parent->children);

}

/*===========================================================================*
 *                             process_name_copy                             *
 *===========================================================================*/
static inline void process_name_copy(struct process *dest,
        struct process *src) {

    strcpy(dest->name, src->name);

}

/*===========================================================================*
 *                              process_fs_copy                              *
 *===========================================================================*/
static inline int process_fs_copy(struct process *dest,
        struct process *src, int clone_flags) {

    if (clone_flags & CLONE_FS) {
        dest->fs = src->fs;
        dest->fs->count++;
    } else {
        if (CONSTRUCT(dest->fs)) return 1;
        memcpy(dest->fs, src->fs, sizeof(struct fs));
        dest->fs->count = 1;
    }

    return 0;

}

/*===========================================================================*
 *                            process_files_copy                             *
 *===========================================================================*/
static inline int process_files_copy(struct process *dest,
        struct process *src, int clone_flags) {

    if (clone_flags & CLONE_FILES) {
        dest->files = src->files;
        dest->files->count++;
    } else {
        if (CONSTRUCT(dest->files)) return 1;
        dest->files->count = 1;
        memcpy(dest->files->files, src->files->files,
                sizeof(struct file *) * PROCESS_FILES);
    }

    return 0;

}

/*===========================================================================*
 *                           process_signals_copy                            *
 *===========================================================================*/
static inline int process_signals_copy(struct process *dest,
        struct process *src, int clone_flags) {

    if (clone_flags & CLONE_SIGHAND) {
        dest->signals = src->signals;
        dest->signals->count++;
    } else {
        if (CONSTRUCT(dest->signals)) return 1;
        memcpy(dest->signals, src->signals, sizeof(struct signals));
        dest->signals->count = 1;
    }

    return 0;

}

/*===========================================================================*
 *                               process_find                                *
 *===========================================================================*/
int process_find(int pid, struct process **p) {

    struct process *proc;

    list_for_each_entry(proc, &init_process.processes, processes)
        if (proc->pid == pid) {
            *p = proc;
            return 0;
        }

    *p = 0;
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
 *                              process_delete                               *
 *===========================================================================*/
void process_delete(struct process *proc) {

    list_del(&proc->siblings);
    list_del(&proc->children);
    list_del(&proc->processes);
    process_space_free(proc);
    arch_process_free(proc);
    process_fs_exit(proc);
    process_files_exit(proc);
    process_signals_exit(proc);
    DESTRUCT(proc);

}

/*===========================================================================*
 *                               process_clone                               *
 *===========================================================================*/
int process_clone(struct process *parent, struct pt_regs *regs,
        int clone_flags) {

    struct process *child;
    int flags, errno = -ENOMEM;

    if (CONSTRUCT(child))
        goto cannot_create_process;
    if (process_space_setup(child))
        goto cannot_allocate;
    process_struct_init(child);
    child->type = parent->type;
    process_name_copy(child, parent);
    if (process_fs_copy(child, parent, clone_flags))
        goto fs_error;
    if (process_files_copy(child, parent, clone_flags))
        goto files_error;
    if (process_signals_copy(child, parent, clone_flags))
        goto signals_error;
    if (arch_process_copy(child, parent, regs))
        goto arch_error;
    list_add_tail(&child->processes, &init_process.processes);
    process_parent_child_link(parent, child);
    process_forked(parent);

    irq_save(flags);
    process_wake(child);
    irq_restore(flags);
    return child->pid;

arch_error:
    process_signals_exit(child);
signals_error:
    process_files_exit(child);
files_error:
    process_fs_exit(child);
fs_error:
cannot_allocate:
    DESTRUCT(child);
cannot_create_process:
    return errno;

}

/*===========================================================================*
 *                               kernel_process                              *
 *===========================================================================*/
int kernel_process(int (*start)(), void *args, unsigned int flags) {

    int pid;

    if ((pid = clone(flags, 0)) == 0)
        exit(start(args));

    return pid;

}

/*===========================================================================*
 *                            process_find_free_fd                           *
 *===========================================================================*/
int process_find_free_fd(struct process *proc, int *fd) {

    int i;
    struct file *dummy;

    /* Find first free file descriptor */
    for (i=0; i<PROCESS_FILES; i++)
        if (process_fd_get(proc, i, &dummy)) break;

    if (dummy) {
        *fd = 0;
        return 1;
    }

    *fd = i;

    return 0;

}

/*===========================================================================*
 *                                  sys_fork                                 *
 *===========================================================================*/
int sys_fork(struct pt_regs regs) {

    /*
     * Not quite compatible with POSIX, but does its work
     */

    return process_clone(process_current, &regs, 0);

}

/*===========================================================================*
 *                                  sys_exit                                 *
 *===========================================================================*/
int sys_exit(int return_value) {

    process_current->exit_code = return_value;
    strcpy(process_current->name, "<defunct>");
    process_exit(process_current);

    return 0;

}

/*===========================================================================*
 *                                 sys_getpid                                *
 *===========================================================================*/
int sys_getpid() {
    return process_current->pid;
}

/*===========================================================================*
 *                                sys_getppid                                *
 *===========================================================================*/
int sys_getppid() {
    return process_current->ppid;
}

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

