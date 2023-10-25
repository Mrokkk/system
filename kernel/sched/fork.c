#include <kernel/vm.h>
#include <kernel/sys.h>
#include <kernel/procfs.h>
#include <kernel/process.h>

unsigned int total_forks;
pid_t last_pid;

static inline pid_t find_free_pid()
{
    return ++last_pid;
}

static inline void process_forked(struct process* proc)
{
    ++proc->forks;
    ++total_forks;
}

vm_area_t* stack_create(uint32_t address, pgd_t* pgd)
{
    int errno;
    vm_area_t* stack_vma;

    page_t* pages = page_alloc(USER_STACK_SIZE / PAGE_SIZE, PAGE_ALLOC_CONT);

    if (unlikely(!pages))
    {
        return NULL;
    }

    stack_vma = vm_create(
        address,
        USER_STACK_SIZE,
        VM_WRITE | VM_READ);

    if (unlikely(!stack_vma))
    {
        goto error;
    }

    errno = vm_map(stack_vma, pages, pgd, 0);

    if (unlikely(errno))
    {
        goto error;
    }

    return stack_vma;

error:
    {
        list_head_t head = LIST_INIT(head);
        list_merge(&head, &pages->list_entry);
        page_range_free(&head);
    }
    return NULL;
}

static int process_space_copy(struct process* dest, struct process* src, int clone_flags)
{
    int errno = -ENOMEM;
    void* pgd;
    vm_area_t* new_vma;
    vm_area_t* src_vma;
    vm_area_t* src_stack_vma;
    vm_area_t* dest_stack_vma;
    page_t* kernel_stack_page;
    uint32_t* kernel_stack_end;

    if (clone_flags & CLONE_MM)
    {
        // TODO: support cloning mm
        return -EINVAL;
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

    mutex_init(&dest->mm->lock);
    dest->mm->pgd = pgd;
    dest->mm->kernel_stack = ptr(addr(kernel_stack_end) + PAGE_SIZE);
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

    if (process_is_kernel(dest))
    {
        // Kernel process does not have any areas, only exec can drop such
        // process to user space, thus allocation is done there
        return 0;
    }

    mutex_lock(&src->mm->lock);

    src_stack_vma = process_stack_vm_area(src);

    // Copy all vma except for stack
    for (src_vma = src->mm->vm_areas; src_vma; src_vma = src_vma->next)
    {
        if (src_vma == src_stack_vma)
        {
            continue;
        }

        new_vma = alloc(vm_area_t);

        if (unlikely(!new_vma))
        {
            goto free_areas;
        }

        errno = vm_copy(new_vma, src_vma, dest->mm->pgd, src->mm->pgd);

        if (unlikely(errno))
        {
            goto free_areas;
        }

        vm_add(&dest->mm->vm_areas, new_vma);
    }

    mutex_unlock(&src->mm->lock);

    dest_stack_vma = stack_create(USER_STACK_VIRT_ADDRESS, pgd);

    if (unlikely(!dest_stack_vma))
    {
        goto free_pgd;
    }

    vm_add(&dest->mm->vm_areas, dest_stack_vma);

    log_debug(DEBUG_PROCESS, "new areas:", dest->pid);
    vm_print(dest->mm->vm_areas, DEBUG_PROCESS);

    return 0;

free_areas:
    // TODO
free_pgd:
    page_free(pgd);
free_kstack:
    pages_free(kernel_stack_page);
free_mm:
    delete(dest->mm);
error:
    return errno;
}

static inline void process_init(struct process* child, struct process* parent)
{
    list_init(&child->running);
    child->pid = find_free_pid();
    child->stat = PROCESS_ZOMBIE;
    child->exit_code = 0;
    child->type = parent->type;
    child->context_switches = 0;
    child->forks = 0;
    child->sid = parent->sid;
    child->need_resched = false;
    strcpy(child->name, parent->name);
    wait_queue_head_init(&child->wait_child);
    list_init(&child->children);
    list_init(&child->siblings);
}

static inline void process_parent_child_link(struct process* parent, struct process* child)
{
    child->parent = parent;
    child->ppid = parent->pid;
    list_add(&child->siblings, &parent->children);
}

static inline void fs_init(struct fs* dest, struct fs* src)
{
    copy_struct(dest, src);
    dest->count = 1;
}

static inline int process_fs_copy(struct process* child, struct process* parent, int clone_flags)
{
    if (clone_flags & CLONE_FS)
    {
        child->fs = parent->fs;
        child->fs->count++;
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
    dest->count = 1;
}

static inline int process_files_copy(struct process* child, struct process* parent, int clone_flags)
{
    if (clone_flags & CLONE_FILES)
    {
        child->files = parent->files;
        child->files->count++;
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
    dest->count = 1;
}

static inline int process_signals_copy(struct process* child, struct process* parent, int clone_flags)
{
    if (clone_flags & CLONE_SIGHAND)
    {
        child->signals = parent->signals;
        child->signals->count++;
        return 0;
    }

    if (!(child->signals = alloc(struct signals, signals_init(this, parent->signals))))
    {
        return -ENOMEM;
    }

    return 0;
}

static inline int process_bin_clone(struct process* child, struct process* parent)
{
    if (parent->bin)
    {
        ++parent->bin->count;
    }
    child->bin = parent->bin;
    return 0;
}

int process_clone(struct process* parent, struct pt_regs* regs, int clone_flags)
{
    int errno = -ENOMEM;
    struct process* child;

    log_debug(DEBUG_PROCESS, "parent: %x:%s[%u]", parent, parent->name, parent->pid);

    cli();

    if (!(child = alloc(struct process, process_init(this, parent)))) goto cannot_create_process;
    if (process_space_copy(child, parent, clone_flags)) goto cannot_allocate;
    if (process_fs_copy(child, parent, clone_flags)) goto fs_error;
    if (process_files_copy(child, parent, clone_flags)) goto files_error;
    if (process_signals_copy(child, parent, clone_flags)) goto signals_error;
    if (process_bin_clone(child, parent)) goto arch_error;
    if (arch_process_copy(child, parent, regs)) goto arch_error;

    list_add_tail(&child->processes, &init_process.processes);
    process_parent_child_link(parent, child);
    process_forked(parent);

    child->stat = PROCESS_RUNNING;
    list_add_tail(&child->running, &running);

    parent->need_resched = true;

    sti();

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
    sti();
    return errno;
}

int kernel_process_spawn(int (*entry)(), void* args, void*, int flags)
{
    int errno = -ENOMEM;
    struct process* child;
    struct process* parent = process_current;

    cli();

    if (!(child = alloc(struct process, process_init(this, parent)))) goto cannot_create_process;
    child->type = KERNEL_PROCESS;
    if (process_space_copy(child, parent, 0)) goto cannot_allocate;
    if (process_fs_copy(child, parent, 0)) goto fs_error;
    if (process_files_copy(child, parent, 0)) goto files_error;
    if (process_signals_copy(child, parent, 0)) goto signals_error;
    if (process_bin_clone(child, parent)) goto arch_error;
    if (arch_process_spawn(child, entry, args, flags)) goto arch_error;

    list_add_tail(&child->processes, &init_process.processes);
    process_parent_child_link(parent, child);
    process_forked(parent);

    child->stat = PROCESS_RUNNING;
    list_add_tail(&child->running, &running);

    sti();
    scheduler();

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
    sti();
    return errno;
}

int sys_fork(struct pt_regs regs)
{
    log_debug(DEBUG_PROCESS, "");
    return process_clone(process_current, &regs, 0);
}

void processes_stats_print(void)
{
    struct process* proc;

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