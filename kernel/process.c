#include <kernel/process.h>
#include <kernel/sys.h>
#include <arch/register.h>
#include <arch/page.h>

PROCESS_DECLARE(init_process);
LIST_DECLARE(running);

static pid_t last_pid;
unsigned int total_forks;

static inline pid_t find_free_pid() {
    /*
     * Just incrementing PID. Maybe it should be more
     * complicated (some hash table maybe?)
     */
    return ++last_pid;
}

static inline void process_forked(struct process *proc) {
    ++proc->forks;
    ++total_forks;
}

static inline void process_space_free(struct process *proc) {
    page_free(proc->mm.start);
}

static inline int process_space_setup(struct process *proc) {

    char *start, *end;

    /* Set up process space */
    if (!(end = start = (char *)page_alloc()))
        return -ENOMEM;

    end += PAGE_SIZE;
    proc->mm.start = start;
    proc->mm.end = end;

    return 0;

}

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

static inline void process_parent_child_link(struct process *parent,
        struct process *child) {

    child->parent = parent;
    child->ppid = parent->pid;
    list_add(&child->siblings, &parent->children);

}

static inline void process_name_copy(struct process *dest,
        struct process *src) {

    strcpy(dest->name, src->name);

}

static inline int process_fs_copy(struct process *dest,
        struct process *src, int clone_flags) {

    if (clone_flags & CLONE_FS) {
        dest->fs = src->fs;
        dest->fs->count++;
    } else {
        if (new(dest->fs)) return 1;
        memcpy(dest->fs, src->fs, sizeof(struct fs));
        dest->fs->count = 1;
    }

    return 0;

}

static inline int process_files_copy(struct process *dest,
        struct process *src, int clone_flags) {

    if (clone_flags & CLONE_FILES) {
        dest->files = src->files;
        dest->files->count++;
    } else {
        if (new(dest->files)) return 1;
        dest->files->count = 1;
        memcpy(dest->files->files, src->files->files,
                sizeof(struct file *) * PROCESS_FILES);
    }

    return 0;

}

static inline int process_signals_copy(struct process *dest,
        struct process *src, int clone_flags) {

    if (clone_flags & CLONE_SIGHAND) {
        dest->signals = src->signals;
        dest->signals->count++;
    } else {
        if (new(dest->signals)) return 1;
        memcpy(dest->signals, src->signals, sizeof(struct signals));
        dest->signals->count = 1;
    }

    return 0;

}

int process_find(int pid, struct process **p) {

    struct process *proc;

    for_each_process(proc)
        if (proc->pid == pid) {
            *p = proc;
            return 0;
        }

    *p = 0;
    return 1;

}

void process_wake_waiting(struct process *proc) {

    struct process *temp;

    if (list_empty(&proc->wait_queue)) return;

    list_for_each_entry(temp, &proc->wait_queue, wait_queue)
        process_wake(temp);

}

void process_delete(struct process *proc) {

    list_del(&proc->siblings);
    list_del(&proc->children);
    list_del(&proc->processes);
    process_space_free(proc);
    arch_process_free(proc);
    process_fs_exit(proc);
    process_files_exit(proc);
    process_signals_exit(proc);
    delete(proc);

}

int process_clone(struct process *parent, struct pt_regs *regs,
        int clone_flags) {

    struct process *child;
    int errno = -ENOMEM;

    if (new(child))
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

    process_wake(child);

    return child->pid;

arch_error:
    process_signals_exit(child);
signals_error:
    process_files_exit(child);
files_error:
    process_fs_exit(child);
fs_error:
cannot_allocate:
    delete(child);
cannot_create_process:
    return errno;

}

int kernel_process(int (*start)(), void *args, unsigned int flags) {

    int pid;

    if ((pid = clone(flags, 0)) == 0)
        exit(start(args));

    return pid;

}

int process_find_free_fd(struct process *proc, int *fd) {

    int i;
    struct file *dummy;

    *fd = 0;

    /* Find first free file descriptor */
    for (i=0; i<PROCESS_FILES; i++)
        if (process_fd_get(proc, i, &dummy)) break;

    if (dummy) return 1;
    *fd = i;
    return 0;

}

int sys_fork(struct pt_regs regs) {

    return process_clone(process_current, &regs, 0);

}

int sys_exit(int return_value) {

    process_current->exit_code = return_value;
    strcpy(process_current->name, "<defunct>");
    process_exit(process_current);

    return 0;

}

int sys_getpid() {
    return process_current->pid;
}

int sys_getppid() {
    return process_current->ppid;
}

int sys_waitpid(int pid, int *status, int opt) {

    struct process *proc;

    (void)opt;

    if (process_find(pid, &proc)) return -ESRCH;
    if (!process_is_running(proc)) goto delete_process;

    list_add_tail(&process_current->wait_queue, &proc->wait_queue);
    process_wait(process_current);

    *status = proc->exit_code;

delete_process:

    if (process_is_zombie(proc))
        process_delete(proc);

    return 0;

}

int processes_init() {

    /*
     * Just add init_process to the running queue
     */
    list_add_tail(&init_process.running, &running);

    return 0;

}

