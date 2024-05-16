#pragma once

#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/wait.h>
#include <kernel/mutex.h>
#include <kernel/usyms.h>
#include <kernel/binary.h>
#include <kernel/dentry.h>
#include <kernel/kernel.h>
#include <kernel/signal.h>

#define STACK_MAGIC 0xdeadc0de
#define PROCESS_FILES       32
#define PROCESS_NAME_LEN    32

typedef enum a
{
    PROCESS_RUNNING = 0,
    PROCESS_WAITING = 1,
    PROCESS_STOPPED = 2,
    PROCESS_ZOMBIE  = 3,
} stat_t;

#define PROCESS_STATE_STRING "RWSZ"

#define EXITCODE(ret, sig)  ((ret) << 8 | (sig))
#define WSTOPPED            0x7f
#define WNOHANG             1
#define WUNTRACED           2
#define WCONTINUED          4

typedef enum
{
    USER_PROCESS        = 1,
    KERNEL_PROCESS      = 2,
} task_type_t;

#define CLONE_FS            (1 << 0)
#define CLONE_FILES         (1 << 1)
#define CLONE_SIGHAND       (1 << 2)
#define CLONE_MM            (1 << 3)

#define SPAWN_KERNEL        0
#define SPAWN_USER          (1 << 1)
#define SPAWN_VM86          (1 << 2)

#define USER_STACK_SIZE             (2 * PAGE_SIZE)
#define USER_STACK_VIRT_ADDRESS     (KERNEL_PAGE_OFFSET - USER_STACK_SIZE)

struct mm
{
    mutex_t lock;
    uint32_t code_start, code_end;
    uint32_t stack_start, stack_end;
    uint32_t args_start, args_end;
    uint32_t env_start, env_end;
    uint32_t brk;
    void* kernel_stack;
    pgd_t* pgd;
    vm_area_t* vm_areas;
#define MM_INIT(mm) \
    { MUTEX_INIT(mm.lock), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

struct signals
{
    int count;
    uint32_t trapped;
    uint32_t pending;
    sighandler_t sighandler[NSIGNALS];
    sigrestorer_t sigrestorer;
    struct signal_context context;
#define SIGNALS_INIT \
    { 1, 0, 0, { 0, }, 0, { 0, }, }
};

struct fs
{
    int count;
    dentry_t* cwd;
    dentry_t* root;
#define FS_INIT \
    { 1, NULL, NULL }
};

struct files
{
    int count;
    file_t* files[PROCESS_FILES];
#define FILES_INIT \
    { 1, { 0, } }
};

struct process
{
    struct context context;
    stat_t stat;
    list_head_t running;
    unsigned need_resched;
    pid_t pid, ppid;
    int exit_code;
    uid_t uid;
    gid_t gid;
    int sid;
    task_type_t type;
    unsigned context_switches, forks;
    char name[PROCESS_NAME_LEN];
    struct mm* mm;
    struct fs* fs;
    struct files* files;
    struct signals* signals;
    struct process* parent;
    binary_t* bin;
    wait_queue_head_t wait_child;
    list_head_t children;
    // Don't iterate over those lists
    list_head_t siblings;
    list_head_t processes;
};

#define PROCESS_STACK_DECLARE(name) \
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
    PROCESS_STACK_DECLARE(name); \
    PROCESS_MM_DECLARE(name); \
    PROCESS_FS_DECLARE(name); \
    PROCESS_FILES_DECLARE(name); \
    PROCESS_SIGNALS_DECLARE(name); \
    struct process name = PROCESS_INIT(name)

extern unsigned* need_resched;
extern struct process init_process;
extern struct process* process_current;

extern list_head_t running;

extern pid_t last_pid;
extern unsigned int total_forks;
extern unsigned int context_switches;

int processes_init();
int process_clone(struct process* parent, struct pt_regs* regs, int clone_flags);
void process_delete(struct process* p);
int kernel_process_spawn(int (*entry)(), void* args, void* stack, int flags);
int process_find(int pid, struct process** p);
void process_wake_waiting(struct process* p);
int process_find_free_fd(struct process* p, int* fd);
void scheduler();
int exit(int);
int fork(void);
void processes_stats_print(void);
int do_exec(const char* pathname, const char* const argv[]);
vm_area_t* stack_create(uint32_t address, pgd_t* pgd);
char* process_print(const struct process* p, char* str);
vm_area_t* exec_prepare_initial_vma();

// Arch-dependent functions
int arch_process_copy(struct process* dest, struct process* src, struct pt_regs* old_regs);
int arch_process_spawn(struct process* child, int (*entry)(), void* args, int flags);
void arch_process_free(struct process* p);
int arch_process_execute_sighan(struct process* p, uint32_t ip, uint32_t restorer);
int arch_exec(void* entry, uint32_t* kernel_stack, uint32_t user_stack);

static inline char process_state_char(int s)
{
    return PROCESS_STATE_STRING[s];
}

static inline int process_is_running(struct process* p)
{
    return p->stat == PROCESS_RUNNING;
}

static inline int process_is_stopped(struct process* p)
{
    return p->stat == PROCESS_STOPPED;
}

static inline int process_is_zombie(struct process* p)
{
    return p->stat == PROCESS_ZOMBIE;
}

static inline void process_exit(struct process* p)
{
    flags_t flags;
    irq_save(flags);
    log_debug(DEBUG_EXIT, "%u exiting", p->pid);
    list_del(&p->running);
    p->stat = PROCESS_ZOMBIE;
    process_wake_waiting(p);
    p->need_resched = true;
    irq_restore(flags);
}

static inline void process_stop(struct process* p)
{
    flags_t flags;
    irq_save(flags);
    list_del(&p->running);
    p->stat = PROCESS_STOPPED;
    process_wake_waiting(p);
    irq_restore(flags);
    if (p == process_current)
    {
        *need_resched = true;
    }
}

static inline void process_wake(struct process* p)
{
    flags_t flags;
    if (p->stat == PROCESS_ZOMBIE)
    {
        log_warning("process %u:%x is zombie", p->pid, p);
        return;
    }
    irq_save(flags);
    if (p->stat != PROCESS_RUNNING)
    {
        list_add_tail(&p->running, &running);
    }
    p->stat = PROCESS_RUNNING;
    irq_restore(flags);
}

static inline int process_wait(wait_queue_head_t* wq, wait_queue_t* q)
{
    flags_t flags;

    if (process_current->stat == PROCESS_ZOMBIE)
    {
        log_warning("process %u:%x is zombie", process_current->pid, process_current);
        scheduler();
        return 0;
    }

    irq_save(flags);

    log_debug(DEBUG_EXIT, "%u waiting", process_current->pid);

    wait_queue_push(q, wq);
    list_del(&process_current->running);
    process_current->stat = PROCESS_WAITING;

    irq_restore(flags);

    scheduler();

    irq_save(flags);

    log_debug(DEBUG_PROCESS, "woken %u", process_current->pid);
    wait_queue_remove(q, wq);

    irq_restore(flags);

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

static inline void process_fd_set(struct process* p, int fd, struct file* file)
{
    p->files->files[fd] = file;
}

static inline int process_fd_get(struct process* p, int fd, struct file** file)
{
    return !(*file = p->files->files[fd]);
}

static inline int fd_check_bounds(int fd)
{
    return (fd >= PROCESS_FILES) || (fd < 0);
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
    if (!--p->files->count)
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

static inline void process_fs_exit(struct process* p)
{
    if (!--p->fs->count) delete(p->fs);
}

static inline void process_bin_exit(struct process* p)
{
    binary_t* bin = p->bin;
    if (!bin)
    {
        return;
    }

    if (!--bin->count)
    {
        if (bin->symbols_pages)
        {
            pages_free(bin->symbols_pages);
        }
        delete(bin);
    }
}

static inline int current_can_kill(struct process* p)
{
    return p->pid != 0;
}

static inline vm_area_t* process_code_vm_area(struct process* p)
{
    uint32_t code_start = p->mm->code_start;
    for (vm_area_t* temp = p->mm->vm_areas; temp; temp = temp->next)
    {
        if (temp->start == code_start)
        {
            return temp;
        }
    }
    return NULL;
}

static inline vm_area_t* process_stack_vm_area(struct process* p)
{
    uint32_t stack_start = p->mm->stack_start;
    for (vm_area_t* vma = p->mm->vm_areas; vma; vma = vma->next)
    {
        if (vma->start == stack_start)
        {
            return vma;
        }
    }
    return NULL;
}

static inline vm_area_t* process_brk_vm_area(struct process* p)
{
    uint32_t brk = p->mm->brk;
    for (vm_area_t* vma = p->mm->vm_areas; vma; vma = vma->next)
    {
        if (vma->start < brk && vma->end >= brk)
        {
            return vma;
        }
    }
    return NULL;
}

#define for_each_process(p) \
    list_for_each_entry(p, &init_process.processes, processes)
