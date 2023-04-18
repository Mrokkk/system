#include <arch/vm.h>
#include <arch/register.h>

#include <kernel/sys.h>
#include <kernel/process.h>

static inline void process_space_free(struct process* proc)
{
    vm_area_t* area;
    vm_area_t* prev;
    int last_pgt = -1;
    pgd_t* pgd = proc->mm->pgd;
    uint32_t* kernel_stack_end = ptr(addr(proc->mm->kernel_stack) - PAGE_SIZE);

    log_debug(DEBUG_PROCESS, "pid %u", proc->pid);

    if (unlikely(*kernel_stack_end != STACK_MAGIC))
    {
        panic("kernel stack overflow; stack @ %x, expected: %x; got: %x",
            kernel_stack_end,
            STACK_MAGIC,
            *kernel_stack_end);
    }

    for (area = proc->mm->vm_areas; area; area = area->next)
    {
        vm_remove(area, pgd, 0);

        uint32_t pde_index = pde_index(area->virt_address);
        log_debug(DEBUG_PROCESS, "pde_index=%u, pgd[pde_index] = %x",
            pde_index,
            virt_ptr(pgd[pde_index] & PAGE_ADDRESS));

        if ((int)pde_index != last_pgt) // cast is fine, as pde_index will never be higher than 1023
        {
            void* virt_pgt = virt_ptr(pgd[pde_index] & PAGE_ADDRESS);
            log_debug(DEBUG_PROCESS, "freeing PGT %x at pgd[%u]", virt_pgt, pde_index);
            page_free(virt_pgt);
            last_pgt = pde_index;
        }
    }

    for (vm_area_t* temp = proc->mm->vm_areas; temp;)
    {
        prev = temp;
        temp = temp->next;
        delete(prev);
    }

    log_debug(DEBUG_PROCESS, "freeing pgd");
    page_free(proc->mm->pgd);
    log_debug(DEBUG_PROCESS, "freeing kstack");
    page_free(kernel_stack_end);
    delete(proc->mm);
}

void process_delete(struct process* proc)
{
    log_debug(DEBUG_PROCESS, "");
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

int sys_waitpid(int pid, int* status, int)
{
    struct process* proc;
    flags_t flags;

    log_debug(DEBUG_PROCESS, "%u called for %u", process_current->pid, pid);

    if (process_find(pid, &proc))
    {
        return -ESRCH;
    }

    irq_save(flags);

    if (process_is_zombie(proc))
    {
        *status = proc->exit_code;
        process_delete(proc);
        goto early_finish;
    }

    WAIT_QUEUE_DECLARE(temp, process_current);

    wait_queue_push(&temp, &proc->wait_child);
    process_wait(process_current, flags);

    *status = proc->exit_code;

    if (process_is_zombie(proc))
    {
        process_delete(proc);
    }

    return 0;

early_finish:
    irq_restore(flags);
    return 0;
}

int sys_exit(int return_value)
{
    log_debug(DEBUG_PROCESS, "process pid=%u, %S exited with %d",
        process_current->pid,
        process_current->name,
        return_value);

    process_current->exit_code = EXITCODE(return_value, 0);
    strcpy(process_current->name, "<defunct>");
    process_exit(process_current);
    return 0;
}
