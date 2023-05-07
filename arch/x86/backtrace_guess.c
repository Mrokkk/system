#if !FRAME_POINTER
#include <arch/register.h>

#include <kernel/page.h>
#include <kernel/ksyms.h>
#include <kernel/kernel.h>
#include <kernel/process.h>

void* backtrace_start(void)
{
    return ptr(esp_get());
}

void* backtrace_next(void** stack_ptr)
{
    void* ret;
    uint32_t* stack = *stack_ptr;

    while (!kernel_address(*stack) || !is_kernel_text(*stack))
    {
        stack++;
    }

    ret = ptr(*stack);
    ++stack;
    *stack_ptr = stack;

    return ret;
}

size_t do_backtrace_process(struct process* p, void** buffer, size_t)
{
    void* eip = ptr(p->context.eip);

    *buffer++ = eip;
    *buffer++ = NULL;

    return 2;
}

void backtrace_exception(struct pt_regs* regs)
{
    char buffer[128];
    uint32_t* esp_ptr;
    uint32_t esp_addr = addr(&regs->esp);
    uint32_t* stack_start = process_current->mm->kernel_stack;

    log_exception("backtrace (guessed):");

    ksym_string(buffer, regs->eip);
    log_exception("%s", buffer);
    for (esp_ptr = ptr(esp_addr); esp_ptr < stack_start; ++esp_ptr)
    {
        if (is_kernel_text(*esp_ptr))
        {
            ksym_string(buffer, *esp_ptr);
            log_exception("%s", buffer);
        }
    }
}
#endif
