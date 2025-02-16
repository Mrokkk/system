#include <arch/asm.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/processor.h>
#include <arch/descriptor.h>

#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/path.h>
#include <kernel/debug.h>
#include <kernel/mutex.h>
#include <kernel/signal.h>
#include <kernel/process.h>
#include <kernel/page_table.h>

#define push(val, stack) \
    do { (stack)--; *stack = (typeof(*stack))(val); } while (0)

extern void exit_kernel(void);
static uintptr_t user_process_setup_stack(process_t* dest, process_t*, const pt_regs_t* src_regs);

#ifdef __i386__
static_assert(offsetof(pt_regs_t, ebx) == REGS_EBX);
static_assert(offsetof(pt_regs_t, ecx) == REGS_ECX);
static_assert(offsetof(pt_regs_t, edx) == REGS_EDX);
static_assert(offsetof(pt_regs_t, esi) == REGS_ESI);
static_assert(offsetof(pt_regs_t, edi) == REGS_EDI);
static_assert(offsetof(pt_regs_t, ebp) == REGS_EBP);
static_assert(offsetof(pt_regs_t, eax) == REGS_EAX);
static_assert(offsetof(pt_regs_t, ds) == REGS_DS);
static_assert(offsetof(pt_regs_t, es) == REGS_ES);
static_assert(offsetof(pt_regs_t, fs) == REGS_FS);
static_assert(offsetof(pt_regs_t, gs) == REGS_GS);
static_assert(offsetof(pt_regs_t, error_code) == REGS_EC);
static_assert(offsetof(pt_regs_t, eip) == REGS_EIP);
static_assert(offsetof(pt_regs_t, cs) == REGS_CS);
static_assert(offsetof(pt_regs_t, eflags) == REGS_EFLAGS);
static_assert(offsetof(pt_regs_t, esp) == REGS_ESP);
static_assert(offsetof(pt_regs_t, ss) == REGS_SS);

static void fork_kernel_stack_frame(uintptr_t** kernel_stack, const pt_regs_t* regs)
{
#define pushk(v) push((v), *kernel_stack)
    uintptr_t eflags = regs->eflags | EFL_IF;
    if (regs->cs == USER_CS)
    {
        pushk(USER_DS);     // ss
        pushk(regs->esp);   // esp
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
    pushk(0);           // cr2
    pushk(regs->ebp);   // ebp
    pushk(regs->edi);   // edi
    pushk(regs->esi);   // esi
    pushk(regs->edx);   // edx
    pushk(regs->ecx);   // ecx
    pushk(regs->ebx);   // ebx
#undef pushk
}

static void exec_kernel_stack_frame(uintptr_t** kernel_stack, uintptr_t esp, uintptr_t eip)
{
#define pushk(v) push((v), *kernel_stack)
    pushk(USER_DS); // ss
    pushk(esp);     // esp
    pushk(EFL_IF);  // eflags
    pushk(USER_CS); // cs
    pushk(eip);     // eip
    pushk(0);       // error_code
    pushk(USER_TLS); // gs
    pushk(USER_DS); // fs
    pushk(USER_DS); // es
    pushk(USER_DS); // ds
    (*kernel_stack) -= 8; // eax, cr2, ebp, edi, esi, edx, ecx, ebx
#undef pushk
}

int arch_process_user_spawn(
    process_t* p,
    uintptr_t fn,
    uintptr_t stack,
    uintptr_t tls)
{
    const pt_regs_t regs = {
        .ss = USER_DS,
        .esp = stack,
        .eflags = EFL_IF,
        .cs = USER_CS,
        .eip = fn,
        .gs = USER_TLS,
        .fs = USER_DS,
        .es = USER_DS,
        .ds = USER_DS,
    };

    p->context.esp = user_process_setup_stack(p, NULL, &regs);
    p->context.eip = addr(&exit_kernel);
    p->context.esp0 = addr(p->kernel_stack);
    p->context.esp2 = regs.esp;
    p->context.tls_base = tls;

    return 0;
}

int arch_process_spawn(process_t* child, process_entry_t entry, void* args, int)
{
    uintptr_t* kernel_stack = child->kernel_stack;
    uintptr_t eflags = EFL_IF;

#define pushk(v) push((v), kernel_stack)
    pushk(args);              // args
    pushk(0);                 // ret
    pushk(eflags);            // eflags
    pushk(KERNEL_CS);         // cs
    pushk(entry);             // eip
    pushk(0);                 // error_code
    pushk(KERNEL_DS);         // gs
    pushk(KERNEL_PER_CPU_DS); // fs
    pushk(KERNEL_DS);         // es
    pushk(KERNEL_DS);         // ds
    kernel_stack -= 8;        // eax, cr2, ebp, edi, esi, edx, ecx, ebx
#undef pushk

    child->context.esp0 = 0;
    child->context.esp = addr(kernel_stack);
    child->context.eip = addr(&exit_kernel);
    child->context.tls_base = 0;

    return 0;
}

int arch_process_copy(
    process_t* dest,
    process_t* src,
    const pt_regs_t* src_regs)
{
    dest->context.esp = user_process_setup_stack(dest, src, src_regs);
    dest->context.eip = addr(&exit_kernel);
    dest->context.esp0 = addr(dest->kernel_stack);
    dest->context.esp2 = src_regs->esp;
    dest->context.tls_base = src->context.tls_base;
    return 0;
}

void FASTCALL(NORETURN(__process_switch_bug(void*, void*, uintptr_t value)))
{
    panic("Invalid canary during process switch to %u:%s: %p", process_current->pid, process_current->name, value);
    ASSERT_NOT_REACHED();
}

void FASTCALL(__process_switch(process_t* prev, process_t* next))
{
    scoped_irq_lock();

    tss.esp = next->context.esp;
    tss.esp0 = next->context.esp0;
    tss.esp2 = next->context.esp2;

    descriptor_set_base(gdt_entries, TLS_ENTRY, next->context.tls_base);

    if (next->mm != prev->mm)
    {
        pgd_load(next->mm->pgd);
    }
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

int NORETURN(arch_exec(void* entry, uintptr_t* kernel_stack, uint32_t user_stack))
{
    cli();

    process_current->context.esp0 = addr(process_current->kernel_stack);

    exec_kernel_stack_frame(&kernel_stack, user_stack, addr(entry));

    tss.esp = addr(kernel_stack);
    tss.esp0 = process_current->context.esp0;
    tss.esp2 = addr(user_stack);

    context_set(kernel_stack, &exit_kernel);

    ASSERT_NOT_REACHED();
}

void NORETURN(sys_sigreturn(pt_regs_t))
{
    signal_frame_t* frame = ptr(process_current->context.esp0);

    log_debug(DEBUG_SIGNAL, "%s", signame(frame->sig));

    process_current->context.esp0 = addr(frame->prev);
    process_current->need_signal = !!process_current->signals->ongoing;

    tss.esp0 = addr(frame->prev);

    context_set(frame->context, &exit_kernel);

    ASSERT_NOT_REACHED();
}

void do_signals(pt_regs_t regs)
{
    process_t* proc = process_current;
    uint32_t* user_stack = ptr(proc->context.esp2);
    signal_frame_t frame;
    signal_t* signal;

    mutex_lock(&proc->signals->lock);

    list_for_each_entry_safe(signal, &proc->signals->queue, list_entry)
    {
        int signum = signal->info.si_signo;
        sigaction_t* sigaction = &proc->signals->sighandlers[signum];

        if (!(proc->signals->ongoing & (1 << signum)))
        {
            continue;
        }

        proc->need_signal = false;
        proc->signals->ongoing &= ~(1 << signum);
        list_del(&signal->list_entry);

        current_log_debug(DEBUG_SIGNAL, "running handler for %s: %p; restorer: %p",
            signame(signum),
            sigaction->sa_handler,
            sigaction->sa_restorer);

        frame.sig = signum;
        frame.context = &regs;
        frame.prev = ptr(proc->context.esp0);

        // Signal frames are saved on the current kernel stack frame. With each nested
        // signal  esp0 is moving forward the stack so that previous frames are not
        // destroyed and it's possible to use signal frame pointed by esp0 to unwind it
        // back to original esp0
        proc->context.esp0 = addr(&frame);

        if (sigaction->sa_flags & SA_SIGINFO)
        {
            user_stack = shift(user_stack, -sizeof(signal->info));
            uintptr_t user_siginfo = addr(user_stack);
            memcpy(user_stack, &signal->info, sizeof(signal->info));
            push(NULL, user_stack);
            push(user_siginfo, user_stack);
        }

        push(signum, user_stack);
        push(sigaction->sa_restorer, user_stack);

        tss.esp0 = addr(&frame);
        delete(signal);

        mutex_unlock(&proc->signals->lock);

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
            "mov "ASM_VALUE(USER_TLS)", %%eax;"
            "mov %%eax, %%gs;"
            "iret;"
            :: "m" (user_stack),
               "m" (sigaction->sa_handler)
            : "memory");
    }

    panic("do_signals: called without signal to handle; ongoing: %#x, pending: %#x",
        proc->signals->ongoing,
        proc->signals->pending);

    ASSERT_NOT_REACHED();
}

#else
static void fork_kernel_stack_frame(uintptr_t**, const pt_regs_t*)
{
}

static void exec_kernel_stack_frame(uintptr_t**, uintptr_t, uintptr_t)
{
}

int arch_process_user_spawn(
    process_t*,
    uintptr_t,
    uintptr_t,
    uintptr_t)
{
    UNUSED(fork_kernel_stack_frame);
    UNUSED(exec_kernel_stack_frame);
    UNUSED(user_process_setup_stack);
    return 0;
}

int arch_process_spawn(process_t*, process_entry_t, void*, int)
{
    return 0;
}

int arch_process_copy(
    process_t*,
    process_t*,
    const pt_regs_t*)
{
    return 0;
}
#endif

static uintptr_t user_process_setup_stack(process_t* dest, process_t*, const pt_regs_t* src_regs)
{
    uintptr_t* kernel_stack = dest->kernel_stack;
    fork_kernel_stack_frame(&kernel_stack, src_regs);
    return addr(kernel_stack);
}

void arch_process_free(process_t*)
{
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
        ASSERT(regs.es == USER_DS);
        ASSERT(regs.fs == USER_DS);
        ASSERT(regs.gs == USER_TLS);
    }
}

int sys_execve(pt_regs_t regs)
{
    int errno;

#ifdef __i386__
    const char* pathname = cptr(regs.ebx);
    const char* const* argv = (const char* const*)(regs.ecx);
    const char* const* envp = (const char* const*)(regs.edx);
#else
    const char* pathname = cptr(regs.rbx);
    const char* const* argv = (const char* const*)(regs.rcx);
    const char* const* envp = (const char* const*)(regs.rdx);
#endif

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

int sys_set_thread_area(uintptr_t base)
{
    process_current->context.tls_base = base;
    descriptor_set_base(gdt_entries, TLS_ENTRY, base);

    return 0;
}

int syscall_permission_check(int, pt_regs_t regs)
{
    process_t* p = process_current;

    scoped_mutex_lock(&p->mm->lock);

    if (!p->mm->syscalls_start)
    {
        return 0;
    }

    if (LIKELY(PT_REGS_IP(&regs) >= p->mm->syscalls_start && PT_REGS_IP(&regs) < p->mm->syscalls_end))
    {
        return 0;
    }

    siginfo_t siginfo = {
        .si_code = SI_KERNEL,
        .si_pid = process_current->pid,
        .si_signo = SIGABRT,
    };

    do_kill(p, &siginfo);

    return -EPERM;
}

int arch_process_init(void)
{
    return 0;
}
