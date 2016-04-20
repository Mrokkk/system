#include <kernel/printk.h>
#include <kernel/stdarg.h>
#include <arch/system.h>

static void (*console_print)(const char *string) = 0;
static char printk_buffer[1024];
static int printk_index = 0;

/*===========================================================================*
 *                                  printk                                   *
 *===========================================================================*/
int printk(const char *fmt, ...) {

    char printf_buf[512];
    va_list args;
    int printed;
    unsigned int flags;

    save_flags(flags);
    cli();

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

    restore_flags(flags);

    return printed;

}

/*===========================================================================*
 *                             console_register                              *
 *===========================================================================*/
void console_register(void (*func)(const char *string)) {

    unsigned int flags;

    save_flags(flags);
    cli();

    console_print = func;
    (*console_print)(printk_buffer);

    restore_flags(flags);

}

