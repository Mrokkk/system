#include <stdarg.h>

#include <arch/earlycon.h>

#include <kernel/fs.h>
#include <kernel/page.h>
#include <kernel/time.h>
#include <kernel/minmax.h>
#include <kernel/printk.h>
#include <kernel/reboot.h>
#include <kernel/seq_file.h>
#include <kernel/backtrace.h>

#define JIFFIES     "\e[32m"
#define INFO        "\e[34m"
#define DEBUG       "\e[38;5;245m"
#define ERROR       "\e[31m"
#define WARNING     "\e[33m"
#define RESET       "\e[0m"

#define PANIC_LINE_LEN          256
#define PRINTK_LINE_LEN         512
#define EARLY_PRINTK_BUF_SIZE   (16 * KiB)
#define BUFFER_FULL_MESSAGE     "\n"WARNING"<buffer full; dropping further messages>"RESET
#define PRINTK_INITIALIZED      0xFEED
#define PRINTK_EARLY_INITIALIZED 0xFEEE

typedef int (*printk_write_t)(struct file*, const char*, size_t);

static void (*printk_fallback)(const char *string);
static void (*printk_early)(const char *string);
static printk_write_t printk_write;
static file_t* printk_file;

static int printk_initialized;
static char printk_buffer[EARLY_PRINTK_BUF_SIZE];
static int printk_index;
static int printk_buffer_full;
static const char* prev_color;

static inline void push_entry(char* buffer, int len, int* index, char* logger_buffer, printk_write_t write)
{
    // If console print function is registered just print text using function,
    // else save text to given logger_buffer
    if (write)
    {
        write(printk_file, buffer, len);
    }
    else if (unlikely(printk_fallback))
    {
        printk_fallback(buffer);
    }

    if (printk_early)
    {
        printk_early(buffer);
    }

    if (printk_buffer_full)
    {
        return;
    }
    if (EARLY_PRINTK_BUF_SIZE - *index - strlen(BUFFER_FULL_MESSAGE) <= (size_t)len)
    {
        *index += sprintf(logger_buffer + *index, BUFFER_FULL_MESSAGE);
        printk_buffer_full = 1;
        return;
    }
    *index += sprintf(logger_buffer + *index, "%s", buffer, *index);
}

void __printk(struct printk_entry* entry, const char *fmt, ...)
{
    ts_t ts;
    char buffer[PRINTK_LINE_LEN];
    va_list args;
    int printed;
    const char* filename = entry->file;
    const char* log_color;

    if (
        0
        //|| entry->log_level < LOGLEVEL_NOTICE
        )
    {
        return;
    }

    scoped_irq_lock();

    timestamp_update();
    timestamp_get(&ts);

    switch (entry->log_level)
    {
        case LOGLEVEL_WARN:
            log_color = WARNING;
            break;
        case LOGLEVEL_ERR:
        case LOGLEVEL_CRIT:
            log_color = ERROR;
            break;
        case LOGLEVEL_DEBUG:
            log_color = DEBUG;
            break;
        default:
            log_color = INFO;
            break;
    }

    switch (entry->log_level)
    {
        case LOGLEVEL_DEBUG:
            printed = sprintf(buffer, "\n"JIFFIES"[%8u.%06u] %s%s:%u:%s: ",
                ts.seconds,
                ts.useconds,
                log_color,
                filename,
                entry->line,
                entry->function);
            break;
        case LOGLEVEL_CONT:
            log_color = prev_color;
            printed = sprintf(buffer, prev_color);
            break;
        default:
            printed = sprintf(buffer, "\n"JIFFIES"[%8u.%06u] %s",
                ts.seconds,
                ts.useconds,
                log_color);
            break;
    }

    va_start(args, fmt);
    printed += vsprintf(buffer + printed, fmt, args);
    va_end(args);

    printed += sprintf(buffer + printed, RESET);

    if (printed >= PRINTK_LINE_LEN)
    {
        panic("overflow");
    }

    prev_color = log_color;

    push_entry(buffer, printed, &printk_index, printk_buffer, printk_write);
}

void panic(const char *fmt, ...)
{
    char buf[PANIC_LINE_LEN];
    va_list args;

    cli();

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    panic_mode_enter();

    printk(KERN_CRIT "Kernel panic: %s", buf);

    backtrace_dump(log_critical);

    panic_mode_die();

    for (;; halt());
}

void ensure_printk_will_print(void)
{
    if (printk_initialized == PRINTK_INITIALIZED)
    {
        return;
    }

    printk_initialized = PRINTK_INITIALIZED;
    extern void serial_print(const char *string);
    extern void debug_print(const char* string);
    printk_fallback = &debug_print;
    printk_fallback(printk_buffer);
}

void printk_register(struct file* file)
{
    if (printk_initialized == PRINTK_INITIALIZED)
    {
        return;
    }

    scoped_irq_lock();

    printk_file = file;
    printk_write = file->ops->write;
    printk_initialized = PRINTK_INITIALIZED;
    (*printk_write)(printk_file, printk_buffer, printk_index);
    log_notice("printk: dumped buffer; size = %d", printk_index);
}

void printk_early_register(void (*print)(const char* string))
{
    if (!print)
    {
        printk_early = NULL;
        printk_initialized = 0;
        return;
    }

    printk_initialized = PRINTK_EARLY_INITIALIZED;
    printk_early = print;
}

int syslog_show(seq_file_t* s)
{
    seq_puts(s, printk_buffer);
    return 0;
}
