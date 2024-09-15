#include <kernel/vm.h>
#include <kernel/procfs.h>
#include <kernel/process.h>
#include <kernel/vm_print.h>
#include <kernel/api/unistd.h>

unsigned int total_forks;
pid_t last_pid;

static inline pid_t find_free_pid()
{
    return ++last_pid;
}

static inline void process_forked(process_t* proc)
{
    ++proc->forks;
    ++total_forks;
}

static int process_space_copy(process_t* dest, process_t* src, int clone_flags)
{
    int errno = -ENOMEM;
    void* pgd;
    vm_area_t* new_vma;
    vm_area_t* src_vma;
    page_t* kernel_stack_page;
    uint32_t* kernel_stack_end;

    if (clone_flags & CLONE_VM)
    {
        scoped_mutex_lock(&src->mm->lock);
        dest->mm = src->mm;
        dest->mm->refcount++;

        kernel_stack_page = page_alloc1();

        if (unlikely(!kernel_stack_page))
        {
            log_exception("cannot allocate page for kernel stack");
            goto free_mm;
        }

        kernel_stack_end = page_virt_ptr(kernel_stack_page);
        *kernel_stack_end = STACK_MAGIC;

        dest->kernel_stack = ptr(addr(kernel_stack_end) + PAGE_SIZE);

        return 0;
    }

    dest->mm = alloc(struct mm);

    if (unlikely(!dest->mm))
    {
        log_exception("cannot allocate mm");
        goto error;
    }

    kernel_stack_page = page_alloc1();

    if (unlikely(!kernel_stack_page))
    {
        log_exception("cannot allocate page for kernel stack");
        goto free_mm;
    }

    kernel_stack_end = page_virt_ptr(kernel_stack_page);
    *kernel_stack_end = STACK_MAGIC;

    pgd = pgd_alloc();

    if (unlikely(!pgd))
    {
        log_exception("cannot allocate page for pgd");
        goto free_kstack;
    }

    log_debug(DEBUG_PROCESS, "pgd=%x", pgd);

    dest->kernel_stack = ptr(addr(kernel_stack_end) + PAGE_SIZE);
    mutex_init(&dest->mm->lock);
    dest->mm->refcount = 1;
    dest->mm->pgd = pgd;
    dest->mm->code_start = src->mm->code_start;
    dest->mm->code_end = src->mm->code_end;
    dest->mm->brk = src->mm->brk;
    dest->mm->args_start = src->mm->args_start;
    dest->mm->args_end = src->mm->args_end;
    dest->mm->stack_start = src->mm->stack_start;
    dest->mm->stack_end = src->mm->stack_end;
    dest->mm->env_start = src->mm->env_start;
    dest->mm->env_end = src->mm->env_end;
    dest->mm->vm_areas = NULL;
    dest->mm->brk_vma = NULL;

    if (process_is_kernel(dest))
    {
        // Kernel process does not have any areas, only exec can drop such
        // process to user space, thus allocation is done there
        return 0;
    }

    mutex_lock(&src->mm->lock);

    // Copy all vmas
    for (src_vma = src->mm->vm_areas; src_vma; src_vma = src_vma->next)
    {
        new_vma = alloc(vm_area_t);

        if (unlikely(!new_vma))
        {
            goto free_areas;
        }

        if (src->mm->brk_vma == src_vma)
        {
            dest->mm->brk_vma = new_vma;
        }

        errno = vm_copy(new_vma, src_vma, dest->mm->pgd, src->mm->pgd);

        if (unlikely(errno))
        {
            goto free_areas;
        }

        vm_add(&dest->mm->vm_areas, new_vma);
    }

    mutex_unlock(&src->mm->lock);

    return 0;

free_areas:
    // TODO
    page_free(pgd);
free_kstack:
    pages_free(kernel_stack_page);
free_mm:
    delete(dest->mm);
error:
    return errno;
}

static inline void process_init(process_t* child, process_t* parent)
{
    list_init(&child->running);
    child->pid = find_free_pid();
    child->stat = PROCESS_ZOMBIE;
    child->exit_code = 0;
    child->type = parent->type;
    child->context_switches = 0;
    child->forks = 0;
    child->sid = parent->sid;
    child->pgid = parent->pgid;
    child->need_resched = false;
    child->need_signal = false;
    child->alarm = 0;
    child->procfs_inode = NULL;
    process_name_set(child, parent->name);
    wait_queue_head_init(&child->wait_child);
    list_init(&child->children);
    list_init(&child->siblings);
    list_init(&child->timers);
}

static inline void process_parent_child_link(process_t* parent, process_t* child)
{
    child->parent = parent;
    child->ppid = parent->pid;
    list_add(&child->siblings, &parent->children);
}

static inline void fs_init(struct fs* dest, struct fs* src)
{
    copy_struct(dest, src);
    dest->refcount = 1;
}

static inline int process_fs_copy(process_t* child, process_t* parent, int clone_flags)
{
    if (clone_flags & CLONE_FS)
    {
        parent->fs->refcount++;
        child->fs = parent->fs;
        return 0;
    }

    if (!(child->fs = alloc(struct fs, fs_init(this, parent->fs))))
    {
        return -ENOMEM;
    }

    return 0;
}

static inline void files_init(struct files* dest, struct files* src)
{
    copy_array(dest->files, src->files, PROCESS_FILES);
    for (int i = 0; i < PROCESS_FILES; ++i)
    {
        if (src->files[i])
        {
            ++src->files[i]->count;
        }
    }
    dest->refcount = 1;
}

static inline int process_files_copy(process_t* child, process_t* parent, int clone_flags)
{
    if (clone_flags & CLONE_FILES)
    {
        parent->files->refcount++;
        child->files = parent->files;
        return 0;
    }

    if (!(child->files = alloc(struct files, files_init(this, parent->files))))
    {
        return -ENOMEM;
    }

    return 0;
}

static inline void signals_init(struct signals* dest, struct signals* src)
{
    copy_struct(dest, src);
    dest->refcount = 1;
}

static inline int process_signals_copy(process_t* child, process_t* parent, int clone_flags)
{
    if (clone_flags & CLONE_SIGHAND)
    {
        parent->signals->refcount++;
        child->signals = parent->signals;
        return 0;
    }

    if (!(child->signals = alloc(struct signals, signals_init(this, parent->signals))))
    {
        return -ENOMEM;
    }

    return 0;
}

static int process_fork(process_t* parent, struct pt_regs* regs)
{
    int errno = -ENOMEM;
    process_t* child;
    scoped_irq_lock();

    log_debug(DEBUG_PROCESS, "parent: %x:%s[%u]", parent, parent->name, parent->pid);

    if (!(child = alloc(process_t, process_init(this, parent)))) goto cannot_create_process;
    if (process_space_copy(child, parent, 0)) goto cannot_allocate;
    if (process_fs_copy(child, parent, 0)) goto fs_error;
    if (process_files_copy(child, parent, 0)) goto files_error;
    if (process_signals_copy(child, parent, 0)) goto signals_error;
    if (arch_process_copy(child, parent, regs)) goto arch_error;

    list_add_tail(&child->processes, &init_process.processes);
    process_parent_child_link(parent, child);
    process_forked(parent);

    child->trace = parent->trace & DTRACE_FOLLOW_FORK
        ? parent->trace
        : 0;
    child->stat = PROCESS_RUNNING;
    list_add_tail(&child->running, &running);

    parent->need_resched = true;

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

int sys_clone(int (*fn)(void*), void* stack, int clone_flags, void*, void* tls)
{
    int errno = -ENOMEM;
    process_t* child;
    scoped_irq_lock();

    process_t* parent = process_current;

    log_debug(DEBUG_PROCESS, "parent: %x:%s[%u]", parent, parent->name, parent->pid);

    if (!(child = alloc(process_t, process_init(this, parent)))) goto cannot_create_process;
    if (process_space_copy(child, parent, clone_flags)) goto cannot_allocate;
    if (process_fs_copy(child, parent, clone_flags)) goto fs_error;
    if (process_files_copy(child, parent, clone_flags)) goto files_error;
    if (process_signals_copy(child, parent, clone_flags)) goto signals_error;
    if (arch_process_user_spawn(child, addr(fn), addr(stack), addr(tls))) goto arch_error;

    list_add_tail(&child->processes, &init_process.processes);
    process_parent_child_link(parent, child);
    process_forked(parent);

    child->trace = parent->trace & DTRACE_FOLLOW_FORK
        ? parent->trace
        : 0;
    child->stat = PROCESS_RUNNING;
    list_add_tail(&child->running, &running);

    parent->need_resched = true;

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

process_t* process_spawn(const char* name, process_entry_t entry, void* args, int flags)
{
    int errno = -ENOMEM;

    process_t* child;
    process_t* parent = flags == SPAWN_KERNEL
        ? &init_process
        : process_current;

    cli();

    if (!(child = alloc(process_t, process_init(this, parent)))) goto cannot_create_process;
    child->type = KERNEL_PROCESS;
    if (process_space_copy(child, parent, 0)) goto cannot_allocate;
    if (process_fs_copy(child, parent, 0)) goto fs_error;
    if (process_files_copy(child, parent, 0)) goto files_error;
    if (process_signals_copy(child, parent, 0)) goto signals_error;
    if (arch_process_spawn(child, entry, args, flags)) goto arch_error;

    list_add_tail(&child->processes, &init_process.processes);
    process_parent_child_link(parent, child);
    process_forked(parent);

    process_name_set(child, name);
    child->trace = 0;
    child->stat = PROCESS_RUNNING;
    list_add_tail(&child->running, &running);

    sti();
    scheduler();

    return child;

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
    sti();
    return ptr(errno);
}

int sys_fork(pt_regs_t regs)
{
    log_debug(DEBUG_PROCESS, "");
    return process_fork(process_current, &regs);
}

// dtrace for dummy trace
int sys_dtrace(int flag)
{
    if (flag & ~(DTRACE_FOLLOW_FORK | DTRACE_BACKTRACE))
    {
        return -EINVAL;
    }
    process_current->trace = 1 | flag;
    return 0;
}

void processes_stats_print(void)
{
    process_t* proc;

    log_info("processes stats:");

    list_for_each_entry(proc, &init_process.processes, processes)
    {
        log_info("pid=%d name=%s stat=%c",
            proc->pid,
            proc->name,
            process_state_char(proc->stat));
    }

    log_info("total_forks=%u", total_forks);
    log_info("context_switches=%u", context_switches);
}
