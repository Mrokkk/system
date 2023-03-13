#include <arch/register.h>

#include <kernel/ksyms.h>
#include <kernel/kernel.h>

#if !FRAME_POINTER
void backtrace_dump(void)
{
    char buffer[80];
    uint32_t* esp_ptr = ptr(esp_get());
    uint32_t esp_addr = addr(esp_ptr);
    uint32_t* stack_start = process_current->mm->kernel_stack;

    printk(KERN_ERR "Backtrace (guessed):");

    for (esp_ptr = ptr(esp_addr); esp_ptr < stack_start; ++esp_ptr)
    {
        if (is_kernel_text(*esp_ptr))
        {
            ksym_string(buffer, *esp_ptr);
            printk(KERN_ERR "%s", buffer);
        }
    }
}
#endif
