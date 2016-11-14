#include <kernel/kernel.h>
#include <kernel/process.h>
#include <stdarg.h>
#include <arch/register.h>

/*===========================================================================*
 *                                   panic                                   *
 *===========================================================================*/
void panic(const char *fmt, ...) {

    unsigned int flags;
    char buf[512];
    va_list args;

    irq_save(flags);

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    /* TODO: call stack */

    printk("Kernel panic: %s\n", buf);

    while (1);

}

