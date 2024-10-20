#if FRAME_POINTER
#include <arch/register.h>

#include <kernel/page.h>
#include <kernel/ksyms.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/backtrace.h>

static inline void* backtrace_start(void)
{
    return ptr(bp_get());
}

static void* backtrace_next(void** frame_ptr)
{
    void* ret;
    stack_frame_t* frame = *frame_ptr;

    if (!kernel_address(addr(frame)) || !is_kernel_text(addr(ret = frame->ret - sizeof(uintptr_t))))
    {
        return NULL;
    }

    frame = frame->next;
    *frame_ptr = frame;

    return ret;
}

static inline size_t do_backtrace_process(const process_t* p, void** buffer, size_t count)
{
    void* ret;
    unsigned depth = 0;

    void* ip = ptr(CONTEXT_IP(&p->context) - sizeof(uintptr_t));
    uintptr_t* stack = ptr(CONTEXT_SP(&p->context));

    stack_frame_t frame;
    void* data = &frame;

    if (p == process_current)
    {
        frame.next = ptr(bp_get());
        frame.ret = ptr(ip_get());
    }
    else if (CONTEXT_IP(&p->context) == addr(&exit_kernel))
    {
        pt_regs_t* regs = ptr(stack);
        frame.next = ptr(PT_REGS_BP(regs));
        frame.ret = ptr(ip);
    }
    else if (CONTEXT_IP(&p->context) == addr(&context_restore))
    {
        context_switch_frame_t* regs = ptr(stack);
        frame.next = ptr(PT_REGS_BP(regs));
        frame.ret = ptr(ip);
    }
    else
    {
        pt_regs_t* regs = ptr(stack);
        frame.next = ptr(PT_REGS_BP(regs));
        frame.ret = ptr(ip);
    }

    while ((ret = backtrace_next(&data)) && depth < count)
    {
        buffer[depth] = ptr(ret);
        ++depth;
    }

    return depth;
}

void backtrace_dump(loglevel_t severity)
{
    void* data = backtrace_start();
    void* ret;
    char buffer[BACKTRACE_SYMNAME_LEN];
    unsigned depth = 0;
    log(severity, "backtrace:");
    while ((ret = backtrace_next(&data)) && depth < BACKTRACE_MAX_RECURSION)
    {
        ksym_string(addr(ret), buffer, sizeof(buffer));
        log(severity, "%s", buffer);
        ++depth;
    }
}

void backtrace_process(const process_t* p, int (*print_func)(), void* arg0)
{
    char buffer[BACKTRACE_SYMNAME_LEN];
    void* bt[BACKTRACE_MAX_RECURSION];
    for (size_t i = 0; i < do_backtrace_process(p, bt, BACKTRACE_MAX_RECURSION); ++i)
    {
        ksym_string(addr(bt[i]), buffer, sizeof(buffer));
        print_func(arg0, "%s\n", buffer);
    }
}

void backtrace_exception(const pt_regs_t* regs)
{
    char buffer[BACKTRACE_SYMNAME_LEN];

    // Normally, when doing stacktrace over frames, return address will contain next
    // address after call. This is not true for exceptions - eip will be set to the
    // instruction which triggered the exception. So to counteract subtraction of
    // sizeof(uintptr_t) in below loop, sizeof(uintptr_t) is added
    stack_frame_t last = {.next = ptr(PT_REGS_BP(regs)), .ret = ptr(PT_REGS_IP(regs) + sizeof(uintptr_t))};
    stack_frame_t* frame = &last;
    void* ret;

    log_exception("backtrace:");
    for (int i = 0; frame && i < BACKTRACE_MAX_RECURSION; ++i)
    {
        if (!kernel_address(addr(frame)) || !is_kernel_text(addr(ret = frame->ret - sizeof(uintptr_t))))
        {
            break;
        }
        ksym_string(addr(ret), buffer, sizeof(buffer));
        log_exception("%s", buffer);
        frame = frame->next;
    }
}
#endif
