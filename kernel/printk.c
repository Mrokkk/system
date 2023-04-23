#include <stdarg.h>

#include <arch/system.h>
#include <arch/register.h>

#include <kernel/fs.h>
#include <kernel/page.h>
#include <kernel/time.h>
#include <kernel/printk.h>
#include <kernel/backtrace.h>

#define JIFFIES     "\e[32m"
#define INFO        "\e[34m"
#define DEBUG       "\e[38;5;245m"
#define EXCEPTION   "\e[31m"
#define WARNING     "\e[33m"
#define RESET       "\e[0m"

#define EARLY_PRINTK_BUF_SIZE (8 * KiB)

static void (*printk_fallback)(const char *string);
static int (*printk_write)(struct file*, const char*, int);
static file_t* printk_file;

#define PRINTK_INITIALIZED 0xFEED
static int printk_initialized;
static char printk_buffer[EARLY_PRINTK_BUF_SIZE];
static int printk_index;
static int printk_buffer_full;

static inline void push_entry(
    char* buffer,
    int len,
    int* index,
    char* logger_buffer,
    int (*write)(struct file*, const char*, int))
{
    // If console print function is registered just print text using function,
    // else save text to given logger_buffer
    if (write)
    {
        (*write)(printk_file, buffer, len);
    }
    else if (unlikely(printk_fallback))
    {
        (*printk_fallback)(buffer);
    }
    else
    {
        if (printk_buffer_full)
        {
            return;
        }
        if (EARLY_PRINTK_BUF_SIZE - *index <= (size_t)len)
        {
            *index += sprintf(logger_buffer + *index, WARNING"<buffer full; dropping further messages>\n"RESET);
            printk_buffer_full = 1;
            return;
        }
        *index += sprintf(logger_buffer + *index, "%s", buffer, *index);
    }
}

void __printk(struct printk_entry* entry, const char *fmt, ...)
{
    // TODO: just have in mind that we have such buffer
    char buffer[512];
    va_list args;
    int printed;
    flags_t flags;
    const char* filename = entry->file;

    if (
        0
        //|| entry->log_level < LOGLEVEL_NOTICE
        )
    {
        return;
    }

    irq_save(flags);

    ts_t ts;
    timestamp_get(&ts);

    switch (entry->log_level)
    {
        case LOGLEVEL_WARN:
            printed = sprintf(buffer, JIFFIES"[%8u.%06u] "WARNING"%s:%u:%s: ",
                ts.seconds,
                ts.useconds,
                filename,
                entry->line,
                entry->function);
            break;
        case LOGLEVEL_ERR:
        case LOGLEVEL_CRIT:
            printed = sprintf(buffer, JIFFIES"[%8u.%06u] "EXCEPTION,
                ts.seconds,
                ts.useconds);
            break;
        case LOGLEVEL_DEBUG:
            printed = sprintf(buffer, JIFFIES"[%8u.%06u] "DEBUG"%s:%u:%s: ",
                ts.seconds,
                ts.useconds,
                filename,
                entry->line,
                entry->function);
            break;
        default:
            printed = sprintf(buffer, JIFFIES"[%8u.%06u] "INFO"%s:%u:%s: ",
                ts.seconds,
                ts.useconds,
                filename,
                entry->line,
                entry->function);
            break;
    }

    va_start(args, fmt);
    printed += vsprintf(buffer + printed, fmt, args);
    va_end(args);

    printed += sprintf(buffer + printed, RESET"\n");

    if (printed >= 512)
    {
        panic("overflow");
    }

    push_entry(buffer, printed, &printk_index, printk_buffer, printk_write);

    irq_restore(flags);
}

void panic(const char *fmt, ...)
{
    uint32_t flags;
    char buf[256];
    va_list args;

    irq_save(flags);

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    ensure_printk_will_print();

    printk(KERN_CRIT "Kernel panic: %s", buf);

    backtrace_dump();

    while (1)
    {
        halt();
    }
}

void ensure_printk_will_print(void)
{
    if (printk_initialized != PRINTK_INITIALIZED)
    {
        printk_initialized = PRINTK_INITIALIZED;
        extern void serial_print(const char *string);
        printk_fallback = &serial_print;
        printk_fallback(printk_buffer);
    }
}

void printk_register(struct file* file)
{
    if (printk_initialized == PRINTK_INITIALIZED)
    {
        return;
    }

    flags_t flags;

    irq_save(flags);

    printk_file = file;
    printk_write = file->ops->write;
    printk_initialized = PRINTK_INITIALIZED;
    (*printk_write)(printk_file, printk_buffer, printk_index);
    log_notice("dumped buffer; size = %d", printk_index);

    irq_restore(flags);
}
