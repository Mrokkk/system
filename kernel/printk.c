#include <stdarg.h>

#include <kernel/fs.h>
#include <kernel/time.h>
#include <kernel/debug.h>
#include <kernel/minmax.h>
#include <kernel/printk.h>
#include <kernel/reboot.h>
#include <kernel/seq_file.h>
#include <kernel/backtrace.h>
#include <kernel/api/syslog.h>

#define PANIC_LINE_LEN           256
#define PRINTK_LINE_LEN          256
#define PRINTK_BUFFER_SIZE       (16 * KiB)
#define PRINTK_INITIALIZED       0xFEED
#define PRINTK_EARLY_INITIALIZED 0xFEEE

typedef int (*printk_write_t)(file_t*, const char*, size_t);

static void (*printk_early)(const char *string);
static printk_write_t printk_fallback;
static printk_write_t printk_write;
static file_t* printk_file;
static logseq_t printk_sequence;
static off_t printk_start;
static off_t printk_end;
static off_t printk_gap;
static off_t printk_iterator;
static off_t printk_prev_iterator;
static int printk_initialized;
static char printk_buffer[PRINTK_BUFFER_SIZE];

static void push_entry(char* buffer, size_t len)
{
    const char* start;

    if (printk_iterator > printk_prev_iterator)
    {
        if (printk_end < printk_start)
        {
            start = strchr(buffer + len + 1, '\n');

            printk_start = start
                ? start - printk_buffer + 1
                : 0;
        }

        printk_end += len;
    }
    else
    {
        start = strchr(buffer + len + 1, '\n');

        if (unlikely(!start))
        {
            __builtin_trap();
        }

        printk_end = len;
        printk_start = start - printk_buffer + 1;
    }

    if (likely(printk_write))
    {
        printk_write(printk_file, buffer, len);
    }
    else if (printk_fallback)
    {
        printk_fallback(NULL, buffer, len);
    }

    if (printk_early)
    {
        printk_early(buffer);
    }
}

logseq_t printk(const printk_entry_t* entry, const char* fmt, ...)
{
    timeval_t ts;
    va_list args;
    int printed;

    scoped_irq_lock();

    printk_prev_iterator = printk_iterator;

    if (PRINTK_BUFFER_SIZE - printk_iterator < PRINTK_LINE_LEN)
    {
        printk_gap = printk_iterator;
        printk_iterator = 0;
    }

    char* const buffer = printk_buffer + printk_iterator;
    logseq_t current_seq = printk_sequence;

    timestamp_update();
    timestamp_get(&ts);

    switch (entry->log_level)
    {
        case KERN_DEBUG:
            printed = snprintf(buffer, PRINTK_LINE_LEN, "%u,%u,%u.%06u;%s:%u:%s: ",
                KERN_DEBUG,
                printk_sequence++,
                ts.tv_sec,
                ts.tv_usec,
                entry->file,
                entry->line,
                entry->function);
            break;
        case KERN_CONT:
            printed = snprintf(buffer, PRINTK_LINE_LEN, " ");
            break;
        default:
            printed = snprintf(buffer, PRINTK_LINE_LEN, "%u,%u,%u.%06u;",
                entry->log_level,
                printk_sequence++,
                ts.tv_sec,
                ts.tv_usec);
            break;
    }

    va_start(args, fmt);
    printed += vsnprintf(buffer + printed, PRINTK_LINE_LEN - printed, fmt, args);
    va_end(args);

    printed += snprintf(buffer + printed, PRINTK_LINE_LEN - printed, "\n");

    if (unlikely(printed >= PRINTK_LINE_LEN))
    {
        strcpy(buffer + PRINTK_LINE_LEN - 5, "...\n");
    }

    printk_iterator += printed;

    push_entry(buffer, printed);

    return current_seq;
}

void NORETURN(panic(const char* fmt, ...))
{
    char buf[PANIC_LINE_LEN];
    va_list args;

    panic_mode_enter();

    va_start(args, fmt);
    vsnprintf(buf, PANIC_LINE_LEN, fmt, args);
    va_end(args);

    log(KERN_CRIT, "Kernel panic: %s", buf);

    backtrace_dump(KERN_CRIT);

    panic_mode_die();

    machine_halt();
}

static size_t printk_buffer_dump(file_t* file, const printk_write_t write)
{
    size_t size = 0;

    if (printk_start > printk_end)
    {
        (*write)(file, printk_buffer + printk_start, size = printk_gap - printk_start);
    }

    (*write)(file, printk_buffer, size += printk_end);

    return size;
}

void ensure_printk_will_print(void)
{
    if (printk_initialized == PRINTK_INITIALIZED)
    {
        return;
    }

    scoped_irq_lock();

    extern int debugcon_write(file_t*, const char* buffer, size_t size);

    printk_initialized = PRINTK_INITIALIZED;
    printk_fallback = &debugcon_write;

    printk_buffer_dump(NULL, printk_fallback);
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

    size_t size = printk_buffer_dump(printk_file, printk_write);
    log_notice("printk: dumped buffer; size = %d", size);
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
    if (printk_start > printk_end)
    {
        seq_puts(s, printk_buffer + printk_start);
    }
    seq_puts(s, printk_buffer);
    return 0;
}

int sys__syslog(int priority, int option, const char* message)
{
    UNUSED(option);

    PRINTK_ENTRY(e, priority & 0xf);
    printk(&e, "%s", message);

    return 0;
}
