#pragma once

#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/wait.h>
#include <kernel/mutex.h>
#include <kernel/dentry.h>
#include <kernel/kernel.h>
#include <kernel/signal.h>
#include <kernel/api/wait.h>
#include <kernel/segmexec.h>
#include <kernel/api/sched.h>
#include <kernel/page_alloc.h>

typedef struct process process_t;

#define STACK_MAGIC 0xdeadc0de
#define PROCESS_FILES       32
#define PROCESS_NAME_LEN    16

typedef enum
{
    PROCESS_RUNNING = 0,
    PROCESS_WAITING = 1,
    PROCESS_STOPPED = 2,
    PROCESS_ZOMBIE  = 3,
} stat_t;

#define PROCESS_STATE_STRING "RWSZ"

#define EXITCODE(ret, sig)  ((ret) << 8 | (sig))
#define WSTOPPED            0x7f

typedef enum
{
    USER_PROCESS        = 0,
    KERNEL_PROCESS      = 1,
} task_type_t;

#define SPAWN_KERNEL        0
#define SPAWN_USER          (1 << 1)

#define USER_STACK_SIZE             (256 * KiB)
#if CONFIG_SEGMEXEC
#define USER_STACK_VIRT_ADDRESS     (CODE_START - USER_STACK_SIZE)
#else
#define USER_STACK_VIRT_ADDRESS     (KERNEL_PAGE_OFFSET - USER_STACK_SIZE)
#endif

struct mm
{
    mutex_t    lock;
    int        refcount;
    uintptr_t  code_start, code_end;
    uintptr_t  stack_start, stack_end;
    uintptr_t  args_start, args_end;
    uintptr_t  env_start, env_end;
    uintptr_t  syscalls_start, syscalls_end;
    uintptr_t  brk;
    pgd_t*     pgd;
    vm_area_t* vm_areas;
    vm_area_t* brk_vma;
#define MM_INIT(mm) \
    { \
        .refcount = 1, \
        .lock = MUTEX_INIT(mm.lock), \
    }
};

struct signals
{
    int         refcount;
    uint32_t    ongoing;
    uint32_t    trapped;
    uint32_t    pending;
    list_head_t queue;
    mutex_t     lock;
    sigaction_t sighandlers[NSIGNALS];
#define SIGNALS_INIT(signals) \
    { \
        .refcount = 1, \
        .lock = MUTEX_INIT(signals.lock), \
    }
};

struct fs
{
    int       refcount;
    dentry_t* cwd;
    dentry_t* root;
#define FS_INIT \
    { \
        .refcount = 1, \
    }
};

struct files
{
    int     refcount;
    file_t* files[PROCESS_FILES];
#define FILES_INIT \
    { \
        .refcount = 1, \
    }
};

struct process
{
    // Cacheline 0
    context_t   context;
    unsigned    _need_resched[0];
    unsigned    need_resched:1;
    unsigned    need_signal:1;
    task_type_t type:1;
    stat_t      stat:2;
    int         trace:3;
    list_head_t running;
    unsigned    context_switches;
    unsigned    forks;
    pid_t       pid;
    pid_t       ppid;
    int         pgid;
    int         sid;
    int         exit_code;
    uid_t       uid;
    gid_t       gid;

    // Cacheline 1
    char              name[PROCESS_NAME_LEN];
    void*             kernel_stack;
    struct mm*        mm;
    struct fs*        fs;
    struct files*     files;
    struct signals*   signals;
    process_t*        parent;
    list_head_t       timers;
    wait_queue_head_t wait_child;
    inode_t*          procfs_inode;

    // Cacheline 2
    int         alarm;
    list_head_t children;
    list_head_t siblings;
    list_head_t processes;
};

#define PROCESS_STACK_DECLARE(name) \
    uintptr_t name##_stack[INIT_PROCESS_STACK_SIZE] = { STACK_MAGIC, }

#define PROCESS_MM_DECLARE(name) \
    struct mm name##_mm = MM_INIT(name##_mm)

#define PROCESS_FS_DECLARE(name) \
    struct fs name##_fs = FS_INIT

#define PROCESS_FILES_DECLARE(name) \
    struct files name##_files = FILES_INIT

#define PROCESS_SIGNALS_DECLARE(name) \
    struct signals name##_signals = SIGNALS_INIT(name##_signals)

#define PROCESS_INIT(proc) \
    { \
        .stat       = PROCESS_RUNNING, \
        .context    = INIT_PROCESS_CONTEXT(proc), \
        .mm         = &proc##_mm, \
        .name       = #proc, \
        .fs         = &proc##_fs, \
        .files      = &proc##_files, \
        .signals    = &proc##_signals, \
        .wait_child = WAIT_QUEUE_HEAD_INIT(proc.wait_child), \
        .processes  = LIST_INIT(proc.processes), \
        .running    = LIST_INIT(proc.running), \
        .children   = LIST_INIT(proc.children), \
        .siblings   = LIST_INIT(proc.siblings), \
        .type       = KERNEL_PROCESS, \
    }

#define PROCESS_DECLARE(name) \
    PROCESS_STACK_DECLARE(name); \
    PROCESS_MM_DECLARE(name); \
    PROCESS_FS_DECLARE(name); \
    PROCESS_FILES_DECLARE(name); \
    PROCESS_SIGNALS_DECLARE(name); \
    process_t name = PROCESS_INIT(name)

extern unsigned* need_resched;
extern process_t init_process;
extern process_t* process_current;

extern list_head_t running;

extern pid_t last_pid;
extern unsigned int total_forks;
extern unsigned int context_switches;

typedef void (*process_entry_t)();

int processes_init();
int process_clone(process_t* parent, struct pt_regs* regs, int clone_flags);
void process_exit(process_t* p);
process_t* process_spawn(const char* name, process_entry_t entry, void* args, int flags);
int process_find(int pid, process_t** p);
void process_wake_waiting(process_t* p);
int process_find_free_fd(process_t* p, int* fd);
int process_find_free_fd_at(process_t* p, int at, int* fd);
void scheduler();
void processes_stats_print(void);
int do_exec(const char* pathname, const char* const argv[], const char* const envp[]);

timer_t repeating_wake_schedule(timeval_t timeval);

#define __WAIT(flags) \
    ({ irq_save(flags); process_wait2(flags); })

#define REPEAT_PER(...) \
    repeating_wake_schedule(__VA_ARGS__); \
    for (flags_t flags;; __WAIT(flags))

// Arch-dependent functions
int arch_process_init(void);
int arch_process_copy(process_t* dest, process_t* src, const pt_regs_t* old_regs);
int arch_process_spawn(process_t* child, process_entry_t entry, void* args, int flags);

int arch_process_user_spawn(
    process_t* p,
    uintptr_t fn,
    uintptr_t stack,
    uintptr_t tls);

void arch_process_free(process_t* p);
int arch_exec(void* entry, uintptr_t* kernel_stack, uint32_t user_stack);

static inline void process_name_set(process_t* p, const char* name)
{
    strlcpy(p->name, name, PROCESS_NAME_LEN);
}

static inline char process_state_char(int s)
{
    return PROCESS_STATE_STRING[s];
}

static inline int process_is_running(process_t* p)
{
    return p->stat == PROCESS_RUNNING;
}

static inline int process_is_stopped(process_t* p)
{
    return p->stat == PROCESS_STOPPED;
}

static inline int process_is_zombie(process_t* p)
{
    return p->stat == PROCESS_ZOMBIE;
}

static inline void process_stop(process_t* p)
{
    scoped_irq_lock();

    list_del(&p->running);
    p->stat = PROCESS_STOPPED;
    process_wake_waiting(p);

    if (p == process_current)
    {
        p->need_resched = true;
    }
}

static inline void NONNULL() process_wake(process_t* p)
{
    if (p->stat == PROCESS_ZOMBIE)
    {
        log_warning("process %u:%p is zombie", p->pid, p);
        return;
    }

    scoped_irq_lock();

    if (p->stat != PROCESS_RUNNING)
    {
        list_add_tail(&p->running, &running);
    }

    p->stat = PROCESS_RUNNING;
}

static inline int process_wait(wait_queue_head_t* wq, wait_queue_t* q)
{
    if (process_current->stat == PROCESS_ZOMBIE)
    {
        log_warning("process %u:%p is zombie", process_current->pid, process_current);
        scheduler();
        return 0;
    }

    {
        scoped_irq_lock();

        log_debug(DEBUG_EXIT, "%u waiting", process_current->pid);

        wait_queue_push(q, wq);
        list_del(&process_current->running);
        process_current->stat = PROCESS_WAITING;
    }

    scheduler();

    {
        scoped_irq_lock();

        log_debug(DEBUG_PROCESS, "woken %u", process_current->pid);
        wait_queue_remove(q, wq);
    }

    if (signal_run(process_current))
    {
        return -EINTR;
    }

    return 0;
}

static inline int process_wait_locked(wait_queue_head_t* wq, wait_queue_t* q, flags_t* flags)
{
    wait_queue_push(q, wq);
    list_del(&process_current->running);
    process_current->stat = PROCESS_WAITING;

    irq_restore(*flags);

    scheduler();

    irq_save(*flags);
    log_debug(DEBUG_PROCESS, "woken %u", process_current->pid);
    wait_queue_remove(q, wq);

    if (signal_run(process_current))
    {
        return -EINTR;
    }

    return 0;
}

static inline void process_wait2(flags_t flags)
{
    list_del(&process_current->running);
    process_current->stat = PROCESS_WAITING;
    irq_restore(flags);
    scheduler();
}

static inline void process_fd_set(process_t* p, int fd, struct file* file)
{
    p->files->files[fd] = file;
}

static inline int process_fd_get(process_t* p, int fd, struct file** file)
{
    return !(*file = p->files->files[fd]);
}

static inline int fd_check_bounds(int fd)
{
    return (fd >= PROCESS_FILES) || (fd < 0);
}

static inline int process_is_kernel(process_t* p)
{
    return p->type == KERNEL_PROCESS;
}

static inline int process_is_user(process_t* p)
{
    return p->type == USER_PROCESS;
}

static inline void process_signals_exit(process_t* p)
{
    if (!--p->signals->refcount)
    {
        delete(p->signals);
    }
}

static inline void process_files_exit(process_t* p)
{
    if (!--p->files->refcount)
    {
        for (int i = 0; i < PROCESS_FILES; ++i)
        {
            if (p->files->files[i])
            {
                do_close(p->files->files[i]);
            }
        }
        delete(p->files);
    }
}

static inline void process_fs_exit(process_t* p)
{
    if (!--p->fs->refcount)
    {
        delete(p->fs);
    }
}

static inline int current_can_kill(process_t* p)
{
    return p->pid != 0;
}

static inline vm_area_t* process_code_vm_area(process_t* p)
{
    uintptr_t code_start = p->mm->code_start;
    for (vm_area_t* temp = p->mm->vm_areas; temp; temp = temp->next)
    {
        if (temp->start == code_start)
        {
            return temp;
        }
    }
    return NULL;
}

static inline vm_area_t* process_stack_vm_area(process_t* p)
{
    uintptr_t stack_start = p->mm->stack_start;
    for (vm_area_t* vma = p->mm->vm_areas; vma; vma = vma->next)
    {
        if (vma->start == stack_start)
        {
            return vma;
        }
    }
    return NULL;
}

static inline vm_area_t* process_brk_vm_area(process_t* p)
{
    uintptr_t brk = p->mm->brk;
    for (vm_area_t* vma = p->mm->vm_areas; vma; vma = vma->next)
    {
        if ((brk >= vma->start && brk <= vma->end) || (brk == vma->start && brk == vma->end))
        {
            return vma;
        }
    }
    return NULL;
}

#define PROCESS_FMT        "%s[%u]: "
#define PROCESS_PARAMS(p)  (p)->name, (p)->pid

#define process_log(severity_name, fmt, proc, ...) \
    log_##severity_name(PROCESS_FMT log_fmt(fmt), PROCESS_PARAMS(proc), ##__VA_ARGS__)

#define process_log_debug(flag, fmt, proc, ...) log_debug(flag, PROCESS_FMT fmt, PROCESS_PARAMS(proc), ##__VA_ARGS__)
#define process_log_info(fmt, proc, ...)        process_log(info, fmt, proc, ##__VA_ARGS__)
#define process_log_notice(fmt, proc, ...)      process_log(notice, fmt, proc, ##__VA_ARGS__)
#define process_log_warning(fmt, proc, ...)     process_log(warning, fmt, proc, ##__VA_ARGS__)
#define process_log_error(fmt, proc, ...)       process_log(error, fmt, proc, ##__VA_ARGS__)
#define process_log_exception(fmt, proc, ...)   process_log(exception, fmt, proc, ##__VA_ARGS__)
#define process_log_critical(fmt, proc, ...)    process_log(PROCESS_FMT fmt, proc, ##__VA_ARGS__)

#define current_log_debug(flag, fmt, ...)       log_debug(flag, PROCESS_FMT fmt, PROCESS_PARAMS(process_current), ##__VA_ARGS__)
#define current_log_info(fmt, ...)              process_log_info(fmt, process_current, ##__VA_ARGS__)
#define current_log_notice(fmt, ...)            process_log_notice(fmt, process_current, ##__VA_ARGS__)
#define current_log_warning(fmt, ...)           process_log_warning(fmt, process_current, ##__VA_ARGS__)
#define current_log_error(fmt, ...)             process_log_error(fmt, process_current, ##__VA_ARGS__)
#define current_log_exception(fmt, ...)         process_log_exception(fmt, process_current, ##__VA_ARGS__)
#define current_log_critical(fmt, ...)          process_log_critical(fmt, process_current, ##__VA_ARGS__)

// For explanation of vm_verify* macros, check kernel/vm.h. Below only wraps those
// macros to pass current process vm areas as vma parameter
#define current_vm_verify(flag, data_ptr) \
    vm_verify(flag, data_ptr, process_current->mm->vm_areas)

#define current_vm_verify_array(flag, data_ptr, n) \
    vm_verify_array(flag, data_ptr, n, process_current->mm->vm_areas)

#define current_vm_verify_buf(flag, data_ptr, n) \
    vm_verify_buf(flag, data_ptr, n, process_current->mm->vm_areas)

#define current_vm_verify_string(flag, data_ptr) \
    vm_verify_string(flag, data_ptr, process_current->mm->vm_areas)

#define current_vm_verify_string_limit(flag, data_ptr, limit) \
    vm_verify_string_limit(flag, data_ptr, limit, process_current->mm->vm_areas)

#define for_each_process(p) \
    list_for_each_entry(p, &init_process.processes, processes)
