#include <stdarg.h>

#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/dev.h>
#include <kernel/tty.h>
#include <kernel/time.h>
#include <kernel/debug.h>
#include <kernel/minmax.h>
#include <kernel/printk.h>
#include <kernel/reboot.h>
#include <kernel/process.h>
#include <kernel/seq_file.h>
#include <kernel/spinlock.h>
#include <kernel/backtrace.h>
#include <kernel/api/syslog.h>

#define PANIC_LINE_LEN           256
#define PRINTK_LINE_LEN          256
#define PRINTK_BUFFER_SIZE       (16 * KiB)
#define PRINTK_INITIALIZED       0xfeed

typedef int (*write_t)(tty_t*, const char*, size_t);

struct printk_state
{
    tty_t*     dedicated_tty;
    write_t    fallback_write;
    logseq_t   sequence;
    off_t      start;
    off_t      end;
    off_t      gap;
    off_t      cur;
    off_t      prev_cur;
    loglevel_t prev_loglevel;
    int        initialized;
    spinlock_t lock;
    char       buffer[PRINTK_BUFFER_SIZE];
};

static struct printk_state CACHELINE_ALIGN state;

static void printk_shift_start_end(char* buffer, size_t len)
{
    const char* start;

    if (state.cur > state.prev_cur)
    {
        if (state.end < state.start)
        {
            start = strchr(buffer + len + 1, '\n');

            state.start = start
                ? start - state.buffer + 1
                : 0;
        }

        state.end += len;
    }
    else
    {
        start = strchr(buffer + len + 1, '\n');

        if (unlikely(!start))
        {
            __builtin_trap();
        }

        state.end = len;
        state.start = start - state.buffer + 1;
    }
}

static void printk_emit(loglevel_t log_level, char* buffer, size_t len, int content_start)
{
    if (likely(state.dedicated_tty))
    {
        tty_write_to(state.dedicated_tty, buffer, len);
    }
    else if (state.fallback_write)
    {
        state.fallback_write(NULL, buffer, len);
    }

    UNUSED(log_level); UNUSED(content_start);
#if 1
    if (log_level >= KERN_NOTICE || (state.prev_loglevel >= KERN_NOTICE && log_level == KERN_CONT))
    {
        if (*buffer == ' ')
        {
            tty_write_to_all(buffer + content_start, len - content_start - 1, state.dedicated_tty);
        }
        else
        {
            tty_write_to_all("\n", 1, state.dedicated_tty);
            tty_write_to_all(buffer + content_start, len - content_start - 1, state.dedicated_tty);
        }
    }
#endif
}

logseq_t printk(const printk_entry_t* entry, const char* fmt, ...)
{
    timeval_t ts;
    va_list args;
    int len, content_start;

    scoped_spinlock_irq_lock(&state.lock);

    state.prev_cur = state.cur;

    if (array_size(state.buffer) - state.cur < PRINTK_LINE_LEN)
    {
        state.gap = state.cur;
        state.cur = 0;
    }

    char* const buffer = state.buffer + state.cur;
    logseq_t current_seq = state.sequence;

    timestamp_update();
    timestamp_get(&ts);

    switch (entry->log_level)
    {
        case KERN_DEBUG:
            len = snprintf(buffer, PRINTK_LINE_LEN, "%u,%lu,%u.%06u;%s:%u:%s: ",
                KERN_DEBUG,
                state.sequence++,
                ts.tv_sec,
                ts.tv_usec,
                entry->file,
                entry->line,
                entry->function);
            break;
        case KERN_CONT:
            len = snprintf(buffer, PRINTK_LINE_LEN, " ");
            break;
        default:
            len = snprintf(buffer, PRINTK_LINE_LEN, "%u,%lu,%u.%06u;",
                entry->log_level,
                state.sequence++,
                ts.tv_sec,
                ts.tv_usec);
            break;
    }

    content_start = len;

    va_start(args, fmt);
    len += vsnprintf(buffer + len, PRINTK_LINE_LEN - len, fmt, args);
    va_end(args);

    len += snprintf(buffer + len, PRINTK_LINE_LEN - len, "\n");

    if (unlikely(len >= PRINTK_LINE_LEN))
    {
        strcpy(buffer + PRINTK_LINE_LEN - 5, "...\n");
    }

    state.cur += len;

    printk_shift_start_end(buffer, len);
    printk_emit(entry->log_level, buffer, len, content_start);

    if (entry->log_level != KERN_CONT)
    {
        state.prev_loglevel = entry->log_level;
    }

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

#define BUFFER_DUMP(func, param) \
    ({ \
        size_t size = 0; \
        if (state.start > state.end) \
        { \
            func(param, state.buffer + state.start, size = state.gap - state.start); \
        } \
        func(param, state.buffer, state.end); \
        size += state.end; \
    })

static size_t printk_buffer_dump(tty_t* tty)
{
    return BUFFER_DUMP(tty_write_to, tty);
}

void ensure_printk_will_print(void)
{
    if (state.initialized == PRINTK_INITIALIZED)
    {
        return;
    }

    extern int debugcon_write(tty_t* tty, const char* buffer, size_t size);

    state.fallback_write = &debugcon_write;
    state.initialized = PRINTK_INITIALIZED;

    BUFFER_DUMP(state.fallback_write, NULL);
}

void printk_register(tty_t* tty)
{
    if (state.initialized == PRINTK_INITIALIZED)
    {
        return;
    }

    scoped_irq_lock();

    state.dedicated_tty = tty;
    state.initialized = PRINTK_INITIALIZED;

    size_t size = printk_buffer_dump(state.dedicated_tty);
    log_notice("printk: dumped buffer; size = %d", size);
}

int syslog_show(seq_file_t* s)
{
    if (state.start > state.end)
    {
        seq_puts(s, state.buffer + state.start);
    }
    seq_puts(s, state.buffer);
    return 0;
}

int sys__syslog(int priority, int option, const char* message)
{
    UNUSED(option);

    if (unlikely(current_vm_verify_string(VERIFY_READ, message)))
    {
        return -EFAULT;
    }

    PRINTK_ENTRY(e, priority & 0xf);
    printk(&e, "%s", message);

    return 0;
}

void printk_init(void)
{
    spinlock_init(&state.lock);
}
