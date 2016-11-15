#include <kernel/printk.h>
#include <stdarg.h>
#include <arch/system.h>

static void (*console_print)(const char *string) = 0;
static char printk_buffer[1024];
static int printk_index = 0;

int printk(const char *fmt, ...) {

    char printf_buf[512];
    va_list args;
    int printed;
    unsigned int flags;

    irq_save(flags);

    va_start(args, fmt);
    printed = vsprintf(printf_buf, fmt, args);
    va_end(args);

    /*
     * If console print function is registered just print text using function,
     * else save text to buffer
     */
    if (console_print)
        (*console_print)(printf_buf);
    else
        printk_index += sprintf(printk_buffer+printk_index, "%s", printf_buf);

    irq_restore(flags);

    return printed;

}

void console_register(void (*func)(const char *string)) {

    unsigned int flags;

    irq_save(flags);

    console_print = func;
    (*console_print)(printk_buffer);

    irq_restore(flags);

}

