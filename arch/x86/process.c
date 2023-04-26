#include <arch/io.h>
#include <arch/vm.h>
#include <arch/string.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/processor.h>
#include <arch/descriptor.h>

#include <kernel/fs.h>
#include <kernel/page.h>
#include <kernel/debug.h>
#include <kernel/process.h>

#define push(val, stack) \
    { (stack)--; *stack = (typeof(*stack))(val); }

extern void ret_from_syscall();

static void* stack_copy(
    uint32_t* dest,
    uint32_t* src,
    uint32_t size)
{
    void* dest_begin = ptr(addr(dest) - size);
    void* src_begin = ptr(addr(src) - size);

    return memcpy(dest_begin, src_begin, size);
}

static inline void fork_kernel_stack_frame(
    uint32_t** kernel_stack,
    uint32_t* user_stack,
    struct pt_regs* regs,
    uint32_t ebp)
{
#define pushk(v) push((v), *kernel_stack)
    if (!ebp)
    {
        ebp = regs->ebp;
    }

    uint32_t eflags = regs->eflags | EFL_IF;
    if (regs->cs == USER_CS)
    {
        pushk(USER_DS);     // ss
        pushk(user_stack);  // esp
    }
    pushk(eflags);      // eflags
    pushk(regs->cs);    // cs
    pushk(regs->eip);   // eip
    pushk(regs->gs);    // gs
    pushk(regs->fs);    // fs
    pushk(regs->es);    // es
    pushk(regs->ds);    // ds
    pushk(0)            // eax; return value in child
    pushk(ebp);         // ebp
    pushk(regs->edi);   // edi
    pushk(regs->esi);   // esi
    pushk(regs->edx);   // edx
    pushk(regs->ecx);   // ecx
    pushk(regs->ebx);   // ebx
#undef pushk
}

uint32_t kernel_process_setup_stack(
    struct process* dest,
    struct process* src,
    struct pt_regs* src_regs)
{
    uint32_t* dest_stack = dest->mm->kernel_stack;
    uint32_t* temp;

    // If we'are not changing privilege level, which we don't do in that case
    // (privilege levels in the process and in the handler are the same), we
    // don't have the EIP value in the pt_regs (exactly we have but it points
    // to some random value). But we can figure out its address. If CPU doesn't
    // change privileges in the exception entry, it doesn't change stacks, so
    // it simply pushes appropriate values (EFLAGS, CS and EIP) on the
    // currently used stack. So, the ESP value that has been used in the
    // process before exception is the address 4 bytes after EFLAGS, which
    // is the address of the ESP field in the pt_regs.

    dest_stack = stack_copy(
        dest_stack,
        src->mm->kernel_stack,
        addr(src->mm->kernel_stack) - addr(&src_regs->esp));

    temp = dest_stack;
    dest->context.esp0 = 0;

    // FIXME: this should not be done; ideally, all stacks should be mapped to same
    // virtual address; and they are for user processes, but not the kernel ones
    uint32_t ebp_offset = addr(src->mm->kernel_stack) - src_regs->ebp;
    uint32_t ebp = addr(dest->mm->kernel_stack) - ebp_offset;

    fork_kernel_stack_frame(&dest_stack, temp, src_regs, ebp);

    log_debug(DEBUG_PROCESS, "stack=%x", dest_stack);

    return addr(dest_stack);
}

uint32_t user_process_setup_stack(
    struct process* dest,
    struct process* src,
    struct pt_regs* src_regs)
{
    uint32_t* kernel_stack;
    vm_area_t* src_stack_vma = process_stack_vm_area(src);
    vm_area_t* dest_stack_vma = process_stack_vm_area(dest);

    uint32_t size = vm_virt_end(src_stack_vma) - src_regs->esp;

    // We can use virt addr of user space directly, as it should be mapped to our vm
    uint32_t* src_stack_start = ptr(vm_virt_end(src_stack_vma));
    uint32_t* dest_stack_start = virt_ptr(vm_paddr_end(dest_stack_vma, ptr(dest->mm->pgd)));

    dest->context.esp0 = addr(kernel_stack = dest->mm->kernel_stack);

    log_debug(DEBUG_PROCESS, "src_user_stack=%x size=%u", src_stack_start, size);

    stack_copy(
        ptr(dest_stack_start),
        src_stack_start,
        size);

    log_debug(DEBUG_PROCESS, "copied %u B; new dest esp=%x",
        size,
        dest_stack_vma->virt_address + dest_stack_vma->size - size);

    fork_kernel_stack_frame(
        &kernel_stack,
        ptr(vm_virt_end(dest_stack_vma) - size),
        src_regs,
        0);

    return addr(kernel_stack);
}

int arch_process_copy(
    struct process* dest,
    struct process* src,
    struct pt_regs* src_regs)
{
    dest->context.esp = process_is_kernel(dest)
        ? kernel_process_setup_stack(dest, src, src_regs)
        : user_process_setup_stack(dest, src, src_regs);

    dest->context.eip = addr(&ret_from_syscall);
    dest->context.iomap_offset = IOMAP_OFFSET;
    dest->context.ss0 = KERNEL_DS;

    memcpy(dest->context.io_bitmap, src->context.io_bitmap, IO_BITMAP_SIZE);

    return 0;
}

void arch_process_free(struct process*)
{
}

#define FASTCALL(x) __attribute__((regparm(3))) x

void FASTCALL(__process_switch(struct process*, struct process* next))
{
    // Change TSS and clear busy bit
    uint32_t base = addr(&next->context);
    descriptor_set_base(gdt_entries, FIRST_TSS_ENTRY, base);
    gdt_entries[FIRST_TSS_ENTRY].access &= 0xf9;

    // Reload TSS with the address of process's
    // context structure and load its pgd
    tss_load(FIRST_TSS_ENTRY << 3);
    pgd_load(phys_ptr(next->mm->pgd));
}

void syscall_regs_check(struct pt_regs regs)
{
    ASSERT(cs_get() == KERNEL_CS);
    ASSERT(ds_get() == KERNEL_DS);
    if (process_is_kernel(process_current))
    {
        ASSERT(regs.cs == KERNEL_CS);
        ASSERT(regs.ds == KERNEL_DS);
        ASSERT(regs.gs == KERNEL_DS);
        ASSERT(gs_get() == KERNEL_DS);
        ASSERT(regs.gs == KERNEL_DS);
    }
    else
    {
        ASSERT(regs.cs == USER_CS);
        ASSERT(regs.ds == USER_DS);
        ASSERT(regs.gs == USER_DS);
        ASSERT(regs.gs == USER_DS);
        ASSERT(gs_get() == USER_DS);
    }
}

int sys_clone(struct pt_regs regs)
{
    return process_clone(process_current, &regs, regs.ebx);
}

static inline void exec_kernel_stack_frame(
    uint32_t** kernel_stack,
    uint32_t esp,
    uint32_t eip)
{
#define pushk(v) push((v), *kernel_stack)
    pushk(USER_DS); // ss
    pushk(esp);     // esp
    pushk(EFL_IF);  // eflags
    pushk(USER_CS); // cs
    pushk(eip);     // eip
    pushk(USER_DS); // gs
    pushk(USER_DS); // fs
    pushk(USER_DS); // es
    pushk(USER_DS); // ds
    pushk(0);       // eax
    pushk(0);       // ebp
    pushk(0);       // edi
    pushk(0);       // esi
    pushk(0);       // edx
    pushk(0);       // ecx
    pushk(0);       // ebx
#undef pushk
}

static inline vm_area_t* user_space_create(
    uint32_t address,
    pgd_t* pgd,
    int vm_apply_flags)
{
    int errno;
    vm_area_t* vma;

    page_t* page = page_alloc1();

    if (unlikely(!page))
    {
        return NULL;
    }

    vma = vm_create(
        page,
        address,
        PAGE_SIZE,
        VM_WRITE | VM_READ);

    if (unlikely(!vma))
    {
        goto error;
    }

    errno = vm_apply(vma, pgd, vm_apply_flags);

    if (unlikely(errno))
    {
        goto error;
    }

    return vma;

error:
    page_free(page_virt_ptr(page));
    return NULL;
}

int sys_exec(struct pt_regs regs)
{
    const char* pathname = cptr(regs.ebx);
    const char* const* argv = (const char* const*)(regs.ecx);
    return do_exec(pathname, argv);
}

#define set_context(stack, ip)  \
    do {                        \
        asm volatile(           \
            "movl %0, %%esp;"   \
            "jmp *%1;"          \
            :: "r" (stack),     \
               "r" (ip));       \
    } while (0)

__noreturn int arch_exec(void* entry, uint32_t* kernel_stack, uint32_t user_stack)
{
    flags_t flags;
    irq_save(flags);
    exec_kernel_stack_frame(&kernel_stack, user_stack, addr(entry));
    set_context(kernel_stack, &ret_from_syscall);
    ASSERT_NOT_REACHED();
}

int arch_process_execute_sighan(struct process* proc, uint32_t eip)
{
    extern void __sigreturn();

    int errno;
    uint32_t* kernel_stack;
    uint32_t* sighan_stack;
    uint32_t flags;
    vm_area_t* user_code_area = NULL;
    pgd_t* pgd = ptr(proc->mm->pgd);
    uint32_t sigret_addr = addr(&__sigreturn);
    uint32_t virt_sigret_addr = USER_SIGRET_VIRT_ADDRESS;
    uint32_t virt_stack_addr = proc->context.esp2;

    irq_save(flags);

    sighan_stack = virt_ptr(vm_paddr(virt_stack_addr, pgd));

    log_debug(DEBUG_PROCESS, "user stack = %x", virt_stack_addr);

    user_code_area = vm_create(
        page_range_get(sigret_addr, 1),
        virt_sigret_addr,
        PAGE_SIZE,
        VM_EXEC | VM_READ | VM_NONFREEABLE);

    if (unlikely(!user_code_area))
    {
        irq_restore(flags);
        return -ENOMEM;
    }

    if ((errno = vm_apply(user_code_area, pgd, VM_APPLY_REPLACE_PG)))
    {
        irq_restore(flags);
        return errno;
    }

    proc->signals->user_code = user_code_area;

    kernel_stack = ptr(addr(proc->context.esp) - 1024); // TODO:

    proc->signals->context.esp = proc->context.esp;
    proc->signals->context.esp0 = proc->context.esp0;
    proc->signals->context.esp2 = proc->context.esp2;
    proc->signals->context.eip = proc->context.eip;

    push(process_current->pid, sighan_stack);
    push(virt_sigret_addr, sighan_stack);

    exec_kernel_stack_frame(&kernel_stack, virt_stack_addr - 8, eip);

    proc->context.eip = addr(&ret_from_syscall);
    proc->context.esp = addr(kernel_stack);
    proc->context.esp0 = proc->context.esp;

    irq_restore(flags);

    if (proc == process_current)
    {
        asm volatile(
            "pushl %%ebx;"
            "pushl %%ecx;"
            "pushl %%esi;"
            "pushl %%edi;"
            "pushl %%ebp;"
            "movl $1f, %0;"
            "movl %%esp, %1;"
            "movl %2, %%esp;"
            "jmp *%3;"
            "1:"
            "popl %%ebp;"
            "popl %%edi;"
            "popl %%esi;"
            "popl %%ecx;"
            "popl %%ebx;"
            :: "m" (proc->signals->context.eip),
               "m" (proc->signals->context.esp),
               "r" (kernel_stack),
               "r" (ret_from_syscall)
            : "memory");
    }

    return 0;
}

__noreturn int sys_sigreturn(struct pt_regs)
{
    log_debug(DEBUG_PROCESS, "");

    vm_area_t* area;

    if ((area = process_current->signals->user_code))
    {
        vm_remove(area, ptr(process_current->mm->pgd), 1);
        delete(area);
    }

    log_debug(DEBUG_PROCESS, "setting stack to %x and jumping to %x",
        process_current->signals->context.esp,
        process_current->signals->context.eip);

    process_current->context.esp0 = process_current->signals->context.esp0;

    set_context(
        process_current->signals->context.esp,
        process_current->signals->context.eip);

    ASSERT_NOT_REACHED();
}
