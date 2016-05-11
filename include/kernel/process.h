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

#define process_state_char(x) \
    PROCESS_STATE_STRING[(int)(x)]

#define USER_PROCESS        0
#define KERNEL_PROCESS      1

#define CLONE_STACK         2
#define CLONE_FILES         4
#define CLONE_OTHER         8

struct mm {
    void *start, *end;
    void *args_start, *args_end;
    void *env_start, *env_end;
};

struct signals {
    unsigned short trapped;
    struct sigaction sigaction[16];
    struct context context;
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
    struct file *files[PROCESS_FILES];
    struct signals *signals;
    struct process *parent;
    struct list_head wait_queue;
    struct list_head children;
    /* It's not safe to use this list directly, yet */
    struct list_head siblings;
    struct list_head processes;
    struct list_head running;

};

#define PROCESS_SPACE_INIT(name) \
    unsigned long name##_stack[INIT_PROCESS_STACK_SIZE] = { STACK_MAGIC, };

#define PROCESS_INIT(proc) \
    {                                               \
        context: {INIT_PROCESS_CONTEXT(proc)},      \
        mm: {                                       \
            &proc##_stack[0],                       \
            &proc##_stack[INIT_PROCESS_STACK_SIZE], \
            0, 0, 0, 0                              \
        },                                          \
        priority: 1,                                \
        name: #proc,                                \
        stat: PROCESS_RUNNING,                      \
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
        type: 1,                                    \
    }

#define PROCESS_DECLARE(name) \
    PROCESS_SPACE_INIT(name); \
    struct process name = PROCESS_INIT(name)

extern struct process init_process;
extern struct process *process_current;

extern struct list_head process_list;
extern struct list_head running;

extern unsigned int total_forks;
extern unsigned int context_switches;

int processes_init();
int process_clone(struct process *proc, struct pt_regs *regs, int clone_flags);
void process_delete(struct process *proc);
int kernel_process(int (*start)(), void *args, unsigned int flags);
int process_find(int pid, struct process **p);
void process_wake_waiting(struct process *proc);
pid_t find_free_pid();
int process_find_free_fd(struct process *proc, int *fd);
void scheduler();
void exit(int);
pid_t fork(void);

/* Arch-dependent functions */
int arch_process_copy(struct process *dest, struct process *src, struct pt_regs *old_regs);
void arch_process_free(struct process *proc);
void regs_print(struct pt_regs *regs);

static inline int process_is_running(struct process *p) {
    return p->stat == PROCESS_RUNNING;
}

static inline int process_is_zombie(struct process *p) {
    return p->stat == PROCESS_ZOMBIE;
}

static inline void process_exit(struct process *proc) {
    list_del(&proc->running);
    proc->stat = PROCESS_ZOMBIE;
    process_wake_waiting(proc);
    if (proc == process_current) {
        scheduler();
        while (1);
    }
}

static inline void process_stop(struct process *proc) {
    list_del(&proc->running);
    proc->stat = PROCESS_STOPPED;
    if (proc == process_current) scheduler();
}

static inline void process_wake(struct process *proc) {
    list_add_tail(&proc->running, &running);
    proc->stat = PROCESS_RUNNING;
}

static inline void process_wait(struct process *proc) {
    list_del(&proc->running);
    proc->stat = PROCESS_WAITING;
    if (proc == process_current) scheduler();
}

static inline void process_fd_set(struct process *proc, int fd, struct file *file) {
    proc->files[fd] = file;
}

static inline int process_fd_get(struct process *proc, int fd, struct file **file) {
    if (!(*file = proc->files[fd])) return 1;
    return 0;
}

static inline int fd_check_bounds(int fd) {
    return (fd >= PROCESS_FILES) || (fd < 0);
}

#define processes_list_print(list, member) \
    do { \
        struct process *__proc; \
        list_for_each_entry(__proc, list, member) \
            printk("pid:%d; name:%s; stat:%d\n", __proc->pid, __proc->name, \
                __proc->stat); \
    } while (0)

#define process_type(proc) \
    ((proc)->type)

#define process_is_kernel(proc) \
    process_type(proc)

#define process_is_user(proc) \
    (!process_type(proc))

#define process_memory_start(proc) \
    (proc)->mm.start

#endif /* __PROCESS_H_ */

