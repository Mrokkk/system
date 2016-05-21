#ifndef __PROCESS_H_
#define __PROCESS_H_

#include <kernel/kernel.h>
#include <kernel/wait.h>
#include <kernel/fs.h>
#include <kernel/signal.h>

#define PROCESS_SPACE 8192
#define PROCESS_FILES 128

/* Process states */
#define PROCESS_RUNNING     0
#define PROCESS_WAITING     1
#define PROCESS_STOPPED     2
#define PROCESS_ZOMBIE      3

#define PROCESS_STATE_STRING "rwsz"

#define USER_PROCESS        0
#define KERNEL_PROCESS      1

#define CLONE_FS            (1 << 0)
#define CLONE_FILES         (1 << 1)
#define CLONE_SIGHAND       (1 << 2)

struct mm {
    void *start, *end;
    void *args_start, *args_end;
    void *env_start, *env_end;
#define MM_INIT(start, end) \
    { (void *)start, (void *)end, 0, 0, 0, 0 }
};

struct signals {
    int count;
    unsigned short trapped;
    sighandler_t sighandler[NSIGNALS];
    struct context context;
#define SIGNALS_INIT \
    { 1, 0, { 0, }, { 0, } }
};

struct fs {
    int count;
    struct inode *root, *pwd;
#define FS_INIT \
    { 1, 0, 0 }
};

struct files {
    int count;
    struct file *files[PROCESS_FILES];
#define FILES_INIT \
    { 1, { 0, } }
};

/*=======================*/
/* Process control block */
/*=======================*/
struct process {

    stat_t stat;
    pid_t pid, ppid;
    int pgrp;
    unsigned int priority;
    int exit_code;
    int exit_state;
    char type;
    struct context context;
    int context_switches;
    int forks;
    struct mm mm;
    char name[32];
    struct fs *fs;
    struct files *files;
    struct signals *signals;
    struct process *parent;
    struct list_head wait_queue;
    struct list_head children;
    /* It's not safe to use this list directly, yet */
    struct list_head siblings;
    struct list_head processes;
    struct list_head running;

};

#define PROCESS_SPACE_DECLARE(name) \
    unsigned long name##_stack[INIT_PROCESS_STACK_SIZE] = { STACK_MAGIC, }

#define PROCESS_FS_DECLARE(name) \
    struct fs name##_fs = FS_INIT

#define PROCESS_FILES_DECLARE(name) \
    struct files name##_files = FILES_INIT

#define PROCESS_SIGNALS_DECLARE(name) \
    struct signals name##_signals = SIGNALS_INIT

#define PROCESS_INIT(proc) \
    {                                               \
        stat: PROCESS_RUNNING,                      \
        context: INIT_PROCESS_CONTEXT(proc),        \
        mm: MM_INIT(&proc##_stack[0],               \
            &proc##_stack[INIT_PROCESS_STACK_SIZE]),\
        name: #proc,                                \
        fs: &proc##_fs,                             \
        files: &proc##_files,                       \
        signals: &proc##_signals,                   \
        processes:                                  \
            LIST_INIT(proc.processes),              \
        running:                                    \
            LIST_INIT(proc.running),                \
        wait_queue:                                 \
            LIST_INIT(proc.wait_queue),             \
        children:                                   \
            LIST_INIT(proc.children),               \
        siblings:                                   \
            LIST_INIT(proc.siblings),               \
        type: KERNEL_PROCESS,                       \
    }

#define PROCESS_DECLARE(name) \
    PROCESS_SPACE_DECLARE(name); \
    PROCESS_FS_DECLARE(name); \
    PROCESS_FILES_DECLARE(name); \
    PROCESS_SIGNALS_DECLARE(name); \
    struct process name = PROCESS_INIT(name)

extern struct process init_process;
extern struct process *process_current;

extern struct list_head process_list;
extern struct list_head running;

extern unsigned int total_forks;
extern unsigned int context_switches;

int processes_init();
int process_clone(struct process *parent, struct pt_regs *regs, int clone_flags);
void process_delete(struct process *p);
int kernel_process(int (*start)(), void *args, unsigned int flags);
int process_find(int pid, struct process **p);
void process_wake_waiting(struct process *p);
int process_find_free_fd(struct process *p, int *fd);
void scheduler();
void exit(int);
int fork(void);

/* Arch-dependent functions */
int arch_process_copy(struct process *dest, struct process *src, struct pt_regs *old_regs);
void arch_process_free(struct process *p);
int arch_process_execute(struct process *p, unsigned int ip);
void regs_print(struct pt_regs *regs);

static inline char process_state_char(int s) {
    return PROCESS_STATE_STRING[s];
}

static inline int process_is_running(struct process *p) {
    return p->stat == PROCESS_RUNNING;
}

static inline int process_is_zombie(struct process *p) {
    return p->stat == PROCESS_ZOMBIE;
}

static inline void process_exit(struct process *p) {
    unsigned int flags;
    irq_save(flags);
    list_del(&p->running);
    p->stat = PROCESS_ZOMBIE;
    irq_restore(flags);
    process_wake_waiting(p);
    if (p == process_current) {
        scheduler();
        while (1);
    }
}

static inline void process_stop(struct process *p) {
    unsigned int flags;
    irq_save(flags);
    list_del(&p->running);
    p->stat = PROCESS_STOPPED;
    irq_restore(flags);
    if (p == process_current) scheduler();
}

static inline void process_wake(struct process *p) {
    unsigned int flags;
    irq_save(flags);
    if (p->stat != PROCESS_RUNNING)
        list_add_tail(&p->running, &running);
    p->stat = PROCESS_RUNNING;
    irq_restore(flags);
}

static inline void process_wait(struct process *p) {
    unsigned int flags;
    irq_save(flags);
    list_del(&p->running);
    p->stat = PROCESS_WAITING;
    irq_restore(flags);
    if (p == process_current) scheduler();
}

static inline void process_fd_set(struct process *p, int fd,
        struct file *file) {
    p->files->files[fd] = file;
}

static inline int process_fd_get(struct process *p, int fd,
        struct file **file) {
    if (!(*file = p->files->files[fd])) return 1;
    return 0;
}

static inline int fd_check_bounds(int fd) {
    return (fd >= PROCESS_FILES) || (fd < 0);
}

static inline int process_type(struct process *p) {
    return p->type;
}

static inline int process_is_kernel(struct process *p) {
    return p->type == KERNEL_PROCESS;
}

static inline int process_is_user(struct process *p) {
    return p->type == USER_PROCESS;
}

static inline void process_signals_exit(struct process *p) {
    if (!--p->signals->count)
        DESTRUCT(p->signals);
}

static inline void process_files_exit(struct process *p) {
    if (!--p->files->count)
        DESTRUCT(p->files);
}

static inline void process_fs_exit(struct process *p) {
    if (!--p->fs->count)
        DESTRUCT(p->fs);
}

static inline void *process_memory_start(struct process *p) {
    return p->mm.start;
}

static inline int current_can_kill(struct process *p) {
    return p->pid != 0;
}

#define processes_list_print(list, member) \
    do { \
        struct process *__proc; \
        list_for_each_entry(__proc, list, member) \
            printk("pid:%d; name:%s; stat:%d\n", __proc->pid, __proc->name, \
                __proc->stat); \
    } while (0)

#endif /* __PROCESS_H_ */

