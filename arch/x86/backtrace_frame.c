#if FRAME_POINTER
#include <arch/register.h>

#include <kernel/page.h>
#include <kernel/ksyms.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/backtrace.h>

void* backtrace_start(void)
{
    return ptr(ebp_get());
}

void* backtrace_next(void** frame_ptr)
{
    void* ret;
    stack_frame_t* frame = *frame_ptr;

    if (!kernel_address(addr(frame)) || !is_kernel_text(addr(frame->ret)))
    {
        return NULL;
    }

    ret = frame->ret;
    frame = frame->next;
    *frame_ptr = frame;

    return ret;
}

size_t do_backtrace_process(struct process* p, void** buffer, size_t count)
{
    void* ret;
    unsigned depth = 0;
    void* eip = ptr(p->context.eip);
    uint32_t* stack = ptr(p->context.esp);

    stack_frame_t frame;
    void* data = &frame;

    if (p == process_current)
    {
        frame.next = ptr(ebp_get());
        frame.ret = ptr(eip_get());
    }
    else if (p->context.eip == addr(&exit_kernel))
    {
        struct pt_regs* regs = ptr(stack);
        frame.next = ptr(regs->ebp);
        frame.ret = ptr(eip);
    }
    else if (p->context.eip == addr(&context_restore))
    {
        struct context_switch_frame* regs = ptr(stack);
        frame.next = ptr(regs->ebp);
        frame.ret = ptr(eip);
    }
    else
    {
        struct pt_regs* regs = ptr(stack);
        frame.next = ptr(regs->ebp);
        frame.ret = ptr(eip);
    }

    while ((ret = backtrace_next(&data)) && depth < count)
    {
        buffer[depth] = ptr(ret);
        ++depth;
    }

    return depth;
}

void backtrace_exception(struct pt_regs* regs)
{
    char buffer[BACKTRACE_SYMNAME_LEN];

    stack_frame_t last = {.next = ptr(regs->ebp), .ret = ptr(regs->eip)};
    stack_frame_t* frame = &last;

    log_exception("backtrace:");
    for (int i = 0; frame && i < BACKTRACE_MAX_RECURSION; ++i)
    {
        if (!kernel_address(addr(frame)) || !is_kernel_text(addr(frame->ret)))
        {
            break;
        }
        ksym_string(buffer, addr(frame->ret));
        log_exception("%s", buffer);
        frame = frame->next;
    }
}
#endif
