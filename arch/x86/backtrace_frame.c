#include <arch/page.h>
#include <arch/register.h>

#include <kernel/ksyms.h>
#include <kernel/kernel.h>
#include <kernel/backtrace.h>

#if FRAME_POINTER
void backtrace_dump(void)
{
    struct stack_frame
    {
        struct stack_frame* next;
        uint32_t ret;
    };

    char buffer[80];
    struct stack_frame* frame = ptr(ebp_get());

    printk(KERN_ERR "Backtrace:");
    for (int i = 0; frame && i < BACKTRACE_MAX_RECURSION; ++i)
    {
        if (!kernel_address(addr(frame)) || !is_kernel_text(frame->ret))
        {
            break;
        }
        ksym_string(buffer, frame->ret);
        printk(KERN_ERR "%s", buffer);
        frame = frame->next;
    }
}
#endif
