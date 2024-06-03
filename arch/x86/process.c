#include <arch/io.h>
#include <arch/string.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/processor.h>
#include <arch/descriptor.h>

#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/debug.h>
#include <kernel/process.h>

#define push(val, stack) \
    { (stack)--; *stack = (typeof(*stack))(val); }

extern void exit_kernel();

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
    ebp = ebp ? : regs->ebp;
    uint32_t eflags = regs->eflags | EFL_IF;
    if (regs->cs == USER_CS)
    {
        pushk(USER_DS);     // ss
        pushk(user_stack);  // esp
    }
    pushk(eflags);      // eflags
    pushk(regs->cs);    // cs
    pushk(regs->eip);   // eip
    pushk(0);           // error_code
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

    uint32_t size = src_stack_vma->end - src_regs->esp;

    // We can use virt addr of user space directly, as it should be mapped to our vm
    uint32_t* src_stack_start = ptr(src_stack_vma->end);
    uint32_t* dest_stack_start = virt_ptr(vm_paddr_end(dest_stack_vma, ptr(dest->mm->pgd)));

    dest->context.esp0 = addr(kernel_stack = dest->mm->kernel_stack);

    log_debug(DEBUG_PROCESS, "src_user_stack=%x size=%u", src_stack_start, size);

    stack_copy(
        ptr(dest_stack_start),
        src_stack_start,
        size);

    log_debug(DEBUG_PROCESS, "copied %u B; new dest esp=%x",
        size,
        dest_stack_vma->end - size);

    fork_kernel_stack_frame(
        &kernel_stack,
        ptr(dest_stack_vma->end - size),
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

    dest->context.eip = addr(&exit_kernel);
    dest->context.iomap_offset = IOMAP_OFFSET;
    dest->context.ss0 = KERNEL_DS;
    dest->context.esp2 = 0;

    memcpy(dest->context.io_bitmap, src->context.io_bitmap, IO_BITMAP_SIZE);

    return 0;
}

void arch_process_free(struct process*)
{
}

// FIXME: There's a crash happening randomly during ltr instruction
// [       2.549597] kernel: general protection fault #0x28 from 0xc0118d92 in pid 1
// [       2.551467] kernel: error code = 0x28
// [       2.552390] kernel: eax = 0x00000028
// [       2.553327] kernel: ebx = 0xc01e8fbc
// [       2.555486] kernel: ecx = 0x000000c0
// [       2.556776] kernel: edx = 0xc0019400
// [       2.557856] kernel: esi = 0xc0019400
// [       2.558783] kernel: edi = 0xc0019560
// [       2.559689] kernel: esp = 0x0000:0xc011c35c
// [       2.560746] kernel: ebp = 0xc0127248
// [       2.561659] kernel: eip = 0x0008:0xc0118d92
// [       2.562826] kernel: ds = 0x0010; es = 0x0010; fs = 0x0010; gs = 0x0010
// [       2.564619] kernel: eflags = 0x00010282 = (iopl=0 cf pf af zf SF tf IF df of nt RF vm ac id )
// [       2.567076] kernel: CR0 = 0x80010013 = (PE MP em ts ET ne WP am nw cd pg )
// [       2.568991] kernel: CR2 = 0x00000000
// [       2.570726] kernel: CR3 = 0x001e7000
// [       2.571717] kernel: CR4 = 0x00000600 = (vme pvi tsd de pse pae mce pge pce OSFXSR OSXMMEXCPT vmxe smxe pcide osxsave smep smap )
// [       2.574609] backtrace:
// [       2.575240] [<0xc0118d92>] __process_switch+0x22/0x3e
// [       2.576500] [<0xc011c358>] timer_handler+0x48/0x4b
void FASTCALL(__process_switch(struct process*, struct process* next))
{
    // Change TSS and clear busy bit
    descriptor_set_base(gdt_entries, TSS_ENTRY, addr(&next->context));
    gdt_entries[TSS_ENTRY].access &= 0xf9;

    // Reload TSS with the address of process's
    // context structure and load its pgd
    tss_load(tss_selector(0));
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
    pushk(0);       // error_code
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

int sys_execve(struct pt_regs regs)
{
    const char* pathname = cptr(regs.ebx);
    const char* const* argv = (const char* const*)(regs.ecx);
    const char* const* envp = (const char* const*)(regs.edx);
    return do_exec(pathname, argv, envp);
}

int arch_process_spawn(struct process* child, int (*entry)(), void* args, int)
{
    uint32_t* kernel_stack = child->mm->kernel_stack;
    uint32_t eflags = EFL_IF;

#define pushk(v) push((v), kernel_stack)
    pushk(args);        // args
    pushk(0);           // ret
    pushk(eflags);      // eflags
    pushk(KERNEL_CS);   // cs
    pushk(entry);       // eip
    pushk(0);           // error_code
    pushk(KERNEL_DS);   // gs
    pushk(KERNEL_DS);   // fs
    pushk(KERNEL_DS);   // es
    pushk(KERNEL_DS);   // ds
    pushk(0)            // eax; return value in child
    pushk(0);           // ebp
    pushk(0);           // edi
    pushk(0);           // esi
    pushk(0);           // edx
    pushk(0);           // ecx
    pushk(0);           // ebx
#undef pushk

    child->context.esp0 = 0;
    child->context.esp = addr(kernel_stack);
    child->context.eip = addr(&exit_kernel);
    child->context.iomap_offset = IOMAP_OFFSET;
    child->context.ss0 = KERNEL_DS;

    memcpy(child->context.io_bitmap, init_process.context.io_bitmap, IO_BITMAP_SIZE);

    return 0;
}

#define set_context(stack, ip)  \
    do {                        \
        asm volatile(           \
            "movl %0, %%esp;"   \
            "jmp *%1;"          \
            :: "r" (stack),     \
               "r" (ip));       \
    } while (0)

int NORETURN(arch_exec(void* entry, uint32_t* kernel_stack, uint32_t user_stack))
{
    flags_t flags;
    irq_save(flags);
    process_current->context.esp0 = addr(process_current->mm->kernel_stack);
    exec_kernel_stack_frame(&kernel_stack, user_stack, addr(entry));
    set_context(kernel_stack, &exit_kernel);
    ASSERT_NOT_REACHED();
}

int arch_process_execute_sighan(struct process* proc, uint32_t eip, uint32_t restorer)
{
    uint32_t* kernel_stack;
    uint32_t* sighan_stack;
    uint32_t flags;
    pgd_t* pgd = ptr(proc->mm->pgd);
    uint32_t virt_stack_addr = proc->context.esp2;

    irq_save(flags);

    sighan_stack = virt_ptr(vm_paddr(virt_stack_addr, pgd));

    log_debug(DEBUG_SIGNAL, "user stack = %x", virt_stack_addr);

    kernel_stack = ptr(addr(proc->context.esp) - 1024); // TODO:

    proc->signals->context.esp = proc->context.esp;
    proc->signals->context.esp0 = proc->context.esp0;
    proc->signals->context.esp2 = proc->context.esp2;
    proc->signals->context.eip = proc->context.eip;

    push(process_current->pid, sighan_stack);
    push(restorer, sighan_stack);

    exec_kernel_stack_frame(&kernel_stack, virt_stack_addr - 8, eip);

    proc->context.eip = addr(&exit_kernel);
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
               "r" (exit_kernel)
            : "memory");
    }

    return 0;
}

int NORETURN(sys_sigreturn(struct pt_regs))
{
    log_debug(DEBUG_SIGNAL, "setting stack to %x and jumping to %x",
        process_current->signals->context.esp,
        process_current->signals->context.eip);

    process_current->context.esp0 = process_current->signals->context.esp0;
    process_current->context.esp2 = process_current->signals->context.esp2;

    set_context(
        process_current->signals->context.esp,
        process_current->signals->context.eip);

    ASSERT_NOT_REACHED();
}
