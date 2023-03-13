#include <arch/vm.h>
#include <arch/register.h>

#include <kernel/sys.h>
#include <kernel/magic.h>
#include <kernel/process.h>

unsigned int total_forks;
static pid_t last_pid;

static inline pid_t find_free_pid()
{
    return ++last_pid;
}

static inline void process_forked(struct process* proc)
{
    ++proc->forks;
    ++total_forks;
}

vm_area_t* stack_create(
    uint32_t address,
    pgd_t* pgd)
{
    int errno;
    vm_area_t* stack_vma;

    page_t* pages = page_alloc(USER_STACK_SIZE / PAGE_SIZE, PAGE_ALLOC_CONT);

    if (unlikely(!pages))
    {
        return NULL;
    }

    stack_vma = vm_create(
        pages,
        address,
        USER_STACK_SIZE,
        VM_WRITE | VM_READ);

    if (unlikely(!stack_vma))
    {
        goto error;
    }

    errno = vm_apply(stack_vma, pgd, VM_APPLY_REPLACE_PG);

    if (unlikely(errno))
    {
        goto error;
    }

    return stack_vma;

error:
    struct list_head head = LIST_INIT(head);
    list_merge(&head, &pages->list_entry);
    page_range_free(&head);
    return NULL;
}

static inline int process_space_copy(struct process* dest, struct process* src, int clone_flags)
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

    pgd = pgd_alloc(src->mm->pgd);

    if (unlikely(!pgd))
    {
        log_exception("cannot allocate page for pgd");
        goto free_kstack;
    }

    log_debug("pgd=%x", pgd);

    mutex_init(&dest->mm->lock);
    dest->mm->pgd = pgd;
    dest->mm->kernel_stack = ptr(addr(kernel_stack_end) + PAGE_SIZE);
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

    for (src_vma = src->mm->vm_areas; src_vma; src_vma = src_vma->next)
    {
        // Copy all vma except for stack
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

    log_debug("new areas:", dest->pid);
    vm_print(dest->mm->vm_areas);

    return 0;

free_areas:
    // TODO
free_pgd:
    page_free(pgd);
free_kstack:
    page_free(page_virt_ptr(kernel_stack_page));
free_mm:
    delete(dest->mm);
error:
    return errno;
}

static inline void process_struct_init(struct process* proc)
{
    proc->pid = find_free_pid();
    proc->exit_code = 0;
    proc->forks = 0;
    proc->context_switches = 0;
    proc->stat = PROCESS_ZOMBIE;
    *proc->name = 0;
    list_init(&proc->children);
    list_init(&proc->siblings);
    list_init(&proc->running);
    wait_queue_head_init(&proc->wait_child);
}

static inline void process_parent_child_link(
    struct process* parent,
    struct process* child)
{
    child->parent = parent;
    child->ppid = parent->pid;
    list_add(&child->siblings, &parent->children);
}

static inline void process_name_copy(
    struct process* dest,
    struct process* src)
{
    strcpy(dest->name, src->name);
}

static inline int process_fs_copy(
    struct process* dest,
    struct process* src,
    int clone_flags)
{
    if (clone_flags & CLONE_FS)
    {
        dest->fs = src->fs;
        dest->fs->count++;
    }
    else
    {
        if (new(dest->fs))
        {
            return 1;
        }
        copy_struct(dest->fs, src->fs);
        dest->fs->count = 1;
    }

    return 0;
}

static inline int process_files_copy(
    struct process* dest,
    struct process* src,
    int clone_flags)
{
    if (clone_flags & CLONE_FILES)
    {
        dest->files = src->files;
        dest->files->count++;
    }
    else
    {
        if (new(dest->files)) return 1;
        dest->files->count = 1;
        copy_array(
            dest->files->files,
            src->files->files,
            PROCESS_FILES);
    }

    return 0;
}

static inline int process_signals_copy(
    struct process* dest,
    struct process* src,
    int clone_flags)
{
    if (clone_flags & CLONE_SIGHAND)
    {
        dest->signals = src->signals;
        dest->signals->count++;
    }
    else
    {
        if (new(dest->signals)) return 1;
        memcpy(dest->signals, src->signals, sizeof(struct signals));
        dest->signals->count = 1;
        dest->signals->user_stack = NULL;
    }

    return 0;
}

int process_clone(
    struct process* parent,
    struct pt_regs* regs,
    int clone_flags)
{
    log_debug("");
    flags_t flags;
    struct process* child;
    int errno = -ENOMEM;

    // FIXME: remove this, I don't need blocking everything during fork
    irq_save(flags);

    if (new(child)) goto cannot_create_process;

    child->type = parent->type;

    if (process_space_copy(child, parent, clone_flags)) goto cannot_allocate;

    process_struct_init(child);
    process_name_copy(child, parent);

    if (process_fs_copy(child, parent, clone_flags)) goto fs_error;
    if (process_files_copy(child, parent, clone_flags)) goto files_error;
    if (process_signals_copy(child, parent, clone_flags)) goto signals_error;
    if (arch_process_copy(child, parent, regs)) goto arch_error;

    list_add_tail(&child->processes, &init_process.processes);
    process_parent_child_link(parent, child);
    process_forked(parent);

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
    delete(child);
cannot_create_process:
    irq_restore(flags);
    return errno;
}

int kernel_process(int (*start)(), void* args, unsigned int)
{
    int pid;

    if ((pid = clone(0, 0)) == 0)
    {
        exit(start(args));
    }

    return pid;
}

int sys_fork(struct pt_regs regs)
{
    log_debug("");
    return process_clone(process_current, &regs, 0);
}

void processes_stats_print(void)
{
    struct process* proc;

    log_debug("processes stats:");

    list_for_each_entry(proc, &init_process.processes, processes)
    {
        log_debug("pid=%d name=%s stat=%c",
            proc->pid,
            proc->name,
            process_state_char(proc->stat));
    }
    log_debug("total_forks=%u", total_forks);
    log_debug("context_switches=%u", context_switches);
}
