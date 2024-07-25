#include <arch/asm.h>
#include <arch/string.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/processor.h>
#include <arch/descriptor.h>

#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/path.h>
#include <kernel/debug.h>
#include <kernel/signal.h>
#include <kernel/process.h>

#define push(val, stack) \
    do { (stack)--; *stack = (typeof(*stack))(val); } while (0)

extern void exit_kernel();

static inline void fork_kernel_stack_frame(
    uint32_t** kernel_stack,
    uint32_t* user_stack,
    pt_regs_t* regs,
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
    pushk(0);           // eax; return value in child
    pushk(ebp);         // ebp
    pushk(regs->edi);   // edi
    pushk(regs->esi);   // esi
    pushk(regs->edx);   // edx
    pushk(regs->ecx);   // ecx
    pushk(regs->ebx);   // ebx
#undef pushk
}

uint32_t user_process_setup_stack(process_t* dest, process_t*, pt_regs_t* src_regs)
{
    uint32_t* kernel_stack;

    dest->context.esp0 = addr(kernel_stack = dest->mm->kernel_stack);

    fork_kernel_stack_frame(
        &kernel_stack,
        ptr(src_regs->esp),
        src_regs,
        0);

    return addr(kernel_stack);
}

int arch_process_copy(
    process_t* dest,
    process_t* src,
    pt_regs_t* src_regs)
{
    dest->context.esp = user_process_setup_stack(dest, src, src_regs);

    dest->context.eip = addr(&exit_kernel);
    dest->context.iomap_offset = IOMAP_OFFSET;
    dest->context.ss0 = KERNEL_DS;
    dest->context.esp2 = src_regs->esp;

    memcpy(dest->context.io_bitmap, src->context.io_bitmap, IO_BITMAP_SIZE);

    return 0;
}

void arch_process_free(process_t*)
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
void FASTCALL(__process_switch(process_t*, process_t* next))
{
    // Change TSS and clear busy bit
    descriptor_set_base(gdt_entries, TSS_ENTRY, addr(&next->context));
    gdt_entries[TSS_ENTRY].access &= 0xf9;

    // Reload TSS with the address of process's
    // context structure and load its pgd
    tss_load(tss_selector(0));
    pgd_load(next->mm->pgd);
}

void syscall_regs_check(pt_regs_t regs)
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

int sys_clone(pt_regs_t regs)
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

int sys_execve(pt_regs_t regs)
{
    int errno;

    const char* pathname = cptr(regs.ebx);
    const char* const* argv = (const char* const*)(regs.ecx);
    const char* const* envp = (const char* const*)(regs.edx);

    if ((errno = path_validate(pathname)))
    {
        return errno;
    }

    // FIXME: validate properly instead of checking only 4 bytes
    if ((errno = vm_verify_array(VERIFY_READ, argv, 4, process_current->mm->vm_areas)))
    {
        return errno;
    }

    return do_exec(pathname, argv, envp);
}

int arch_process_spawn(process_t* child, process_entry_t entry, void* args, int)
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
    pushk(0);           // eax; return value in child
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

#define context_set(stack, ip)  \
    do                          \
    {                           \
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
    context_set(kernel_stack, &exit_kernel);
    ASSERT_NOT_REACHED();
}

void NORETURN(sys_sigreturn(pt_regs_t))
{
    signal_frame_t* frame = ptr(process_current->context.esp0);

    log_debug(DEBUG_SIGNAL, "%s", signame(frame->sig));

    process_current->context.esp0 = addr(frame->prev);
    process_current->need_signal = !!process_current->signals->ongoing;

    context_set(frame->context, &exit_kernel);

    ASSERT_NOT_REACHED();
}

void do_signals(pt_regs_t regs)
{
    process_t* proc = process_current;
    uint32_t* user_stack = ptr(proc->context.esp2);
    signal_frame_t frame;

    cli();

    for (int sig = 0; sig < NSIGNALS; ++sig)
    {
        if (!(proc->signals->ongoing & (1 << sig)))
        {
            continue;
        }

        proc->need_signal = false;
        proc->signals->ongoing &= ~(1 << sig);

        log_debug(DEBUG_SIGNAL, "%u: running handler for %s", proc->pid, signame(sig));

        frame.sig = sig;
        frame.context = &regs;
        frame.prev = ptr(proc->context.esp0);

        // Signal frames are saved on the current kernel stack frame. With each nested
        // signal  esp0 is moving forward the stack so that previous frames are not
        // destroyed and it's possible to use signal frame pointed by esp0 to unwind it
        // back to original esp0
        proc->context.esp0 = addr(&frame);

        // Currently only void(int sig) signal handler
        // are supported
        push(sig, user_stack);
        push(proc->signals->sigrestorer, user_stack);

        asm volatile(
            "pushl "ASM_VALUE(USER_DS)";" // ss
            "push %0;"                    // esp
            "pushl "ASM_VALUE(EFL_IF)";"  // eflags
            "pushl "ASM_VALUE(USER_CS)";" // cs
            "push %1;"                    // eip
            "mov "ASM_VALUE(USER_DS)", %%eax;"
            "mov %%eax, %%ds;"
            "mov %%eax, %%es;"
            "mov %%eax, %%fs;"
            "mov %%eax, %%gs;"
            "sti;"
            "iret;"
            :: "m" (user_stack),
               "m" (proc->signals->sighandler[sig])
            : "memory");
    }

    ASSERT_NOT_REACHED();
}

int arch_process_init(void)
{
    return 0;
}
