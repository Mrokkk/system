#pragma once

#include <arch/vm.h>
#include <arch/page.h>

#include <kernel/fs.h>
#include <kernel/wait.h>
#include <kernel/mutex.h>
#include <kernel/dentry.h>
#include <kernel/kernel.h>
#include <kernel/signal.h>

#define STACK_MAGIC 0xdeadc0de
#define PROCESS_FILES       32
#define PROCESS_NAME_LEN    32

#define PROCESS_RUNNING     0
#define PROCESS_WAITING     1
#define PROCESS_STOPPED     2
#define PROCESS_ZOMBIE      3

#define PROCESS_STATE_STRING "rwsz"
#define EXITCODE(ret, sig) ((ret) << 8 | (sig))

#define USER_PROCESS        1
#define KERNEL_PROCESS      2

#define CLONE_FS            (1 << 0)
#define CLONE_FILES         (1 << 1)
#define CLONE_SIGHAND       (1 << 2)
#define CLONE_MM            (1 << 3)

// TODO: those shouldn't be hardcoded
#define USER_SIGRET_VIRT_ADDRESS    (KERNEL_PAGE_OFFSET - 5 * PAGE_SIZE)
#define USER_SIGSTACK_VIRT_ADDRESS  (KERNEL_PAGE_OFFSET - 4 * PAGE_SIZE)
#define USER_STACK_VIRT_ADDRESS     (KERNEL_PAGE_OFFSET - 3 * PAGE_SIZE)
#define USER_ARGV_VIRT_ADDRESS      (KERNEL_PAGE_OFFSET - PAGE_SIZE)
#define USER_STACK_SIZE             (2 * PAGE_SIZE)

// Every process has its own kernel stack, user stack (in first
// vm_area; of course not for kernel process) and PGD
struct mm
{
    mutex_t lock;
    uint32_t stack_start, stack_end;
    uint32_t args_start, args_end;
    uint32_t env_start, env_end;
    uint32_t brk;
    void* kernel_stack;
    pgd_t* pgd;
    vm_area_t* vm_areas;
#define MM_INIT(mm) \
    { MUTEX_INIT(mm.lock), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

struct signals
{
    int count;
    unsigned short trapped;
    sighandler_t sighandler[NSIGNALS];
    struct context context;
    vm_area_t* user_stack;
    vm_area_t* user_code;
#define SIGNALS_INIT \
    { 1, 0, { 0, }, { 0, }, 0, 0, }
};

struct fs
{
    int count;
    dentry_t* cwd;
#define FS_INIT \
    { 1, 0 }
};

struct files
{
    int count;
    struct file* files[PROCESS_FILES];
#define FILES_INIT \
    { 1, { 0, } }
};

struct process
{
    struct context context;
    stat_t stat;
    struct list_head running;
    pid_t pid, ppid;
    int exit_code;
    char type;
    unsigned context_switches;
    unsigned forks;
    char name[PROCESS_NAME_LEN];
    struct mm* mm;
    struct fs* fs;
    struct files* files;
    struct signals* signals;
    struct process* parent;
    struct wait_queue_head wait_child;
    struct list_head children;
    // It's not safe to use this list directly, yet; TODO: why?
    struct list_head siblings;
    struct list_head processes;
};

#define PROCESS_SPACE_DECLARE(name) \
    unsigned long name##_stack[INIT_PROCESS_STACK_SIZE] = { STACK_MAGIC, }

#define PROCESS_MM_DECLARE(name) \
    struct mm name##_mm = MM_INIT(name##_mm)

#define PROCESS_FS_DECLARE(name) \
    struct fs name##_fs = FS_INIT

#define PROCESS_FILES_DECLARE(name) \
    struct files name##_files = FILES_INIT

#define PROCESS_SIGNALS_DECLARE(name) \
    struct signals name##_signals = SIGNALS_INIT

#define PROCESS_INIT(proc) \
    { \
        .stat = PROCESS_RUNNING, \
        .context = INIT_PROCESS_CONTEXT(proc), \
        .mm = &proc##_mm, \
        .name = #proc, \
        .fs = &proc##_fs, \
        .files = &proc##_files, \
        .signals = &proc##_signals, \
        .wait_child = WAIT_QUEUE_HEAD_INIT(proc.wait_child), \
        .processes = LIST_INIT(proc.processes), \
        .running = LIST_INIT(proc.running), \
        .children = LIST_INIT(proc.children), \
        .siblings = LIST_INIT(proc.siblings), \
        .type = KERNEL_PROCESS, \
    }

#define PROCESS_DECLARE(name) \
    PROCESS_SPACE_DECLARE(name); \
    PROCESS_MM_DECLARE(name); \
    PROCESS_FS_DECLARE(name); \
    PROCESS_FILES_DECLARE(name); \
    PROCESS_SIGNALS_DECLARE(name); \
    struct process name = PROCESS_INIT(name)

extern struct process init_process;
extern struct process* process_current;

extern struct list_head process_list;
extern struct list_head running;

extern unsigned int total_forks;
extern unsigned int context_switches;

int processes_init();
int process_clone(struct process* parent, struct pt_regs* regs, int clone_flags);
void process_delete(struct process* p);
int kernel_process(int (*start)(), void* args, unsigned int flags);
int process_find(int pid, struct process** p);
void process_wake_waiting(struct process* p);
int process_find_free_fd(struct process* p, int* fd);
void scheduler();
void exit(int);
int fork(void);
void processes_stats_print(void);
int do_exec(const char* pathname, const char* const argv[]);
vm_area_t* stack_create(uint32_t address, pgd_t* pgd);
char* process_print(const struct process* p, char* str);

// Arch-dependent functions
int arch_process_copy(struct process* dest, struct process* src, struct pt_regs* old_regs);
void arch_process_free(struct process* p);
int arch_process_execute_sighan(struct process* p, uint32_t ip);
int arch_exec(void* entry, uint32_t* kernel_stack, uint32_t user_stack);

static inline char process_state_char(int s)
{
    return PROCESS_STATE_STRING[s];
}

static inline int process_is_running(struct process* p)
{
    return p->stat == PROCESS_RUNNING;
}

static inline int process_is_zombie(struct process* p)
{
    return p->stat == PROCESS_ZOMBIE;
}

static inline void process_exit(struct process* p)
{
    flags_t flags;
    irq_save(flags);
    list_del(&p->running);
    p->stat = PROCESS_ZOMBIE;
    process_wake_waiting(p);
    irq_restore(flags);
    if (p == process_current)
    {
        scheduler();
        ASSERT_NOT_REACHED();
    }
}

static inline void process_stop(struct process* p)
{
    flags_t flags;
    irq_save(flags);
    list_del(&p->running);
    p->stat = PROCESS_STOPPED;
    irq_restore(flags);
    if (p == process_current)
    {
        scheduler();
    }
}

static inline void process_wake(struct process* p)
{
    flags_t flags;
    irq_save(flags);
    if (p->stat != PROCESS_RUNNING)
    {
        list_add_tail(&p->running, &running);
    }
    p->stat = PROCESS_RUNNING;
    irq_restore(flags);
}

static inline void process_wait(struct process* p, flags_t flags)
{
    list_del(&p->running);
    p->stat = PROCESS_WAITING;
    if (p == process_current)
    {
        irq_restore(flags);
        scheduler();
    }
    else
    {
        irq_restore(flags);
    }
}

static inline void process_fd_set(
    struct process* p,
    int fd,
    struct file* file)
{
    p->files->files[fd] = file;
}

static inline int process_fd_get(
    struct process* p,
    int fd,
    struct file** file)
{
    return !(*file = p->files->files[fd]);
}

static inline int fd_check_bounds(int fd)
{
    return (fd >= PROCESS_FILES) || (fd < 0);
}

static inline int process_type(struct process* p)
{
    return p->type;
}

static inline int process_is_kernel(struct process* p)
{
    return p->type == KERNEL_PROCESS;
}

static inline int process_is_user(struct process* p)
{
    return p->type == USER_PROCESS;
}

static inline void process_signals_exit(struct process* p)
{
    if (!--p->signals->count) delete(p->signals);
}

static inline void process_files_exit(struct process* p)
{
    if (!--p->files->count) delete(p->files);
}

static inline void process_fs_exit(struct process* p)
{
    if (!--p->fs->count) delete(p->fs);
}

static inline int current_can_kill(struct process* p)
{
    return p->pid != 0;
}

static inline vm_area_t* process_stack_vm_area(struct process* p)
{
    uint32_t stack_start = p->mm->stack_start;
    for (vm_area_t* temp = p->mm->vm_areas; temp; temp = temp->next)
    {
        if (temp->virt_address == stack_start)
        {
            return temp;
        }
    }
    return NULL;
}

static inline vm_area_t* process_brk_vm_area(struct process* p)
{
    uint32_t brk = p->mm->brk;
    for (vm_area_t* temp = p->mm->vm_areas; temp; temp = temp->next)
    {
        if (temp->virt_address < brk && temp->virt_address + temp->size >= brk)
        {
            return temp;
        }
    }
    return NULL;
}

#define for_each_process(p) \
    list_for_each_entry(p, &init_process.processes, processes)
