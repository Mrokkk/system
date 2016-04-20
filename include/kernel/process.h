#ifndef __PROCESS_H_
#define __PROCESS_H_

#include <kernel/kernel.h>
#include <kernel/wait.h>
#include <kernel/fs.h>
#include <kernel/signal.h>

#ifndef INIT_PROCESS_CONTEXT
#define INIT_PROCESS_CONTEXT
#endif

#ifndef INIT_PROCESS_STACK_SIZE
#define INIT_PROCESS_STACK_SIZE 512
#endif

#ifndef INIT_PROCESS_STACK
#define INIT_PROCESS_STACK
#endif

#define INIT_PROCESS_PID 1

#define PROCESS_SPACE 8192
#define PROCESS_FILES 128

/* Process states */
#define NO_PROCESS          0
#define PROCESS_WAITING     1
#define PROCESS_STOPPED     2
#define PROCESS_RUNNING     3

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

    pid_t pid, ppid;
    int pgrp;
    stat_t stat;
    int kernel;
    unsigned int priority;
    int errno;
    struct context context;
    int context_switches;
    int forks;
    /* Memory management structure */
    struct mm mm;
    char *name; /* TODO: Replace with args */
    struct file *files[PROCESS_FILES];
    struct signals *signals;

    /* For sys_wait */
    struct wait_queue *wait_queue;

    /* Pointers to other processes        *
     * 'y' means younger, 'o' means older */
    struct process *parent, *y_child, *o_child,
                   *y_sibling, *o_sibling;
    
    /* Lists */
    struct list_head processes;
    struct list_head queue;

};

#define INIT_PROCESS \
    {                                               \
        context: {INIT_PROCESS_CONTEXT},            \
        mm: {                                       \
            &init_stack[0],                         \
            &init_stack[INIT_PROCESS_STACK_SIZE],   \
            0, 0, 0, 0                              \
        },                                          \
        pid: INIT_PROCESS_PID,                      \
        priority: 1,                                \
        name: "init",                               \
        stat: PROCESS_RUNNING,                      \
        processes:                                  \
            LIST_INIT(init_process.processes),      \
        queue:                                      \
            LIST_INIT(init_process.queue),          \
    }

extern struct process init_process;
extern struct process *process_current;

extern struct list_head process_list;
extern struct list_head running;
extern struct list_head waiting;
extern struct list_head stopped;

extern unsigned int total_forks;
extern unsigned int context_switches;

struct process *process_find(int pid);
int process_stop(struct process *proc);
int process_wake(struct process *proc);
int process_exit(struct process *proc);
pid_t find_free_pid();
void resched();
int kthread_create(int (*start)(), const char *name);

#endif /* __PROCESS_H_ */

