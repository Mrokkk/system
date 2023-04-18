#include <stdarg.h>

#include <arch/system.h>
#include <arch/register.h>

#include <kernel/fs.h>
#include <kernel/page.h>
#include <kernel/printk.h>
#include <kernel/backtrace.h>

#define JIFFIES     "\e[32m"
#define DEBUG       "\e[34m"
#define EXCEPTION   "\e[31m"
#define WARNING     "\e[33m"
#define RESET       "\e[0m"

static void (*printk_fallback)(const char *string);
static int (*printk_write)(struct file*, const char*, int);
static file_t* printk_file;

#define PRINTK_INITIALIZED 0xFEED
static int printk_initialized;
static char printk_buffer[PAGE_SIZE * 4]; // FIXME: may cause random crashes
static int printk_index = 0;

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
        *index += sprintf(logger_buffer + *index, "%s", buffer);
    }
}

int __printk(struct printk_entry* entry, const char *fmt, ...)
{
    // TODO: just have in mind that we have such buffer
    char buffer[768];
    va_list args;
    int printed;
    flags_t flags;
    const char* filename = strrchr(entry->file, '/') + 1;

    if (
        0
        /*|| entry->log_level < LOGLEVEL_NOTICE*/
        )
    {
        return 0;
    }

    irq_save(flags);

    switch (entry->log_level)
    {
        case LOGLEVEL_WARN:
            printed = sprintf(buffer, JIFFIES"[%8u] "WARNING"%s:%u:%s: ",
                jiffies,
                filename,
                entry->line,
                entry->function);
            break;
        case LOGLEVEL_ERR:
        case LOGLEVEL_CRIT:
            printed = sprintf(buffer, JIFFIES"[%8u] "EXCEPTION,
                jiffies);
            break;
        default:
            printed = sprintf(buffer, JIFFIES"[%8u] "DEBUG"%s:%u:%s: ",
                jiffies,
                filename,
                entry->line,
                entry->function);
            break;
    }

    va_start(args, fmt);
    printed += vsprintf(buffer + printed, fmt, args);
    va_end(args);

    printed += sprintf(buffer + printed, RESET"\n");

    push_entry(buffer, printed, &printk_index, printk_buffer, printk_write);

    irq_restore(flags);

    return printed;
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

    irq_restore(flags);
}
