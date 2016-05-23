#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/stdarg.h>
#include <arch/register.h>

#define ebp_get() \
    ({                                              \
        unsigned int rv;                            \
        asm volatile("mov %%ebp, %0" : "=r" (rv) :: "memory");  \
        rv;                                         \
    })

void panic(const char *fmt, ...) {

    unsigned int flags, i, *esp;
    char buf[512];
    va_list args;

    irq_save(flags);

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    printk("Call trace:\n");

    /* Traverse stack */
    for (esp = (unsigned *)(&fmt - 1), i = 0; i < 128; i++, esp++) {
        if (*esp >= (unsigned int)_stext && *esp < (unsigned int)_etext) {
            printk("-%d: 0x%x @ 0x%x\n", i,  *esp, (unsigned int)esp);
        }
    }

    printk("Kernel panic: %s\n", buf);

    while (1);

}
