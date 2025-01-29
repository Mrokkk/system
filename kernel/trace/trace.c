#include <stdarg.h>
#include <kernel/ctype.h>
#include <kernel/trace.h>
#include <kernel/bitset.h>
#include <kernel/kernel.h>
#include <kernel/minmax.h>
#include <kernel/signal.h>
#include <kernel/process.h>
#include <kernel/process.h>
#include <kernel/api/mman.h>
#include <kernel/api/ioctl.h>
#include <kernel/api/types.h>
#include <kernel/api/unistd.h>
#include <kernel/api/syscall.h>

extern syscall_t trace_syscalls[];

static int string_print(const char* string, int limit, char* buffer, const char* end)
{
    char* it = buffer;

    if (unlikely(current_vm_verify_string_limit(VM_READ, string, 64)))
    {
        it = csnprintf(it, end, "%p", string);
        return it - buffer;
    }

    size_t len = limit != -1 ? (size_t)limit : strlen(string);
    size_t to_print = min(len, 64);

    it = csnprintf(it, end, "\"");

    for (size_t i = 0; i < to_print; ++i)
    {
        it = csnprintf(it, end - 5, isprint(string[i]) ? "%c" : "\\%u", string[i] & 0xff);
    }

    if (len > to_print)
    {
        it = csnprintf(it, end, "...");
    }

    it = csnprintf(it, end, "\"");

    return it - buffer;
}

static const char* format_get(type_t t, int value)
{
    switch (t)
    {
        case TYPE_CHAR:
            return isprint(value) ? "\'%c\'" : "\'%#lx\'";
        case TYPE_SHORT:
        case TYPE_LONG: return "%ld";
        case TYPE_CHAR_PTR:
        case TYPE_UNSIGNED_CHAR:
        case TYPE_UNSIGNED_SHORT:
        case TYPE_UNSIGNED_LONG:
            return "%#lx";
        case TYPE_VOID_PTR:
            return "%p";
        case TYPE_VOID:
            return "void";
        default: return "unrecognized{%#lx}";
    }
}

static void* ptr_get(uintptr_t i)
{
    return ptr(i + sizeof(uintptr_t));
}

static long* parameters_get(long* buf, va_list args, size_t count)
{
    if (count > 5)
    {
        return va_arg(args, long*);
    }
    for (size_t i = 0; i < count; ++i)
    {
        buf[i] = va_arg(args, int);
    }
    return buf;
}

#define PRINT(fmt, ...) \
    *it = csnprintf(*it, end, fmt, ##__VA_ARGS__)

static void signal_name(int signum, char** it, const char* end)
{
    PRINT("%s", signame(signum));
}

static void select_fds_print(int nfds, const fd_set_t* fds, char** it, const char* end)
{
    if (unlikely(current_vm_verify(VM_READ, fds)))
    {
        PRINT("%p", fds);
        return;
    }

    PRINT("[");

    for (int i = 0; i < nfds; ++i)
    {
        if (bitset_test((uint32_t*)fds->fd_bits, i))
        {
            PRINT("%s%u", i == 0 ? "" : ", ", i);
        }
    }

    PRINT("]");
}

static void timeval_print(timeval_t* timeval, char** it, const char* end)
{
    if (unlikely(current_vm_verify(VM_READ, timeval)))
    {
        PRINT("%p", timeval);
        return;
    }

    PRINT("{tv_sec = %u, tv_usec = %u}", timeval->tv_sec, timeval->tv_usec);
}

static void timespec_print(timespec_t* timespec, char** it, const char* end)
{
    if (unlikely(current_vm_verify(VM_READ, timespec)))
    {
        PRINT("%p", timespec);
        return;
    }

    PRINT("{tv_sec = %u, tv_nsec = %lu}", timespec->tv_sec, timespec->tv_nsec);
}

#define FLAG(f) \
    case f: PRINT(#f); return 1

#define BITFLAG_EMPTY(v, c) \
    if (!(v)) \
    { \
        PRINT("%s", #c); \
        continuation = 1; \
    }

#define BITFLAG(f) \
    if (value & (f)) \
    { \
        if (continuation) \
        { \
            PRINT("|"); \
        } \
        PRINT(#f); \
        value ^= f; \
        continuation = 1; \
    }

#define BITFLAG_LEFT() \
    if (value) \
    { \
        PRINT("%s%#x", continuation ? "|" : "", value); \
    }

#define FOR_ARGUMENT(nr, ...) \
    do if (id == nr) \
    { \
        __VA_ARGS__; \
    } \
    while (0)

static int special_parameter(int nr, size_t id, int value, long* params, char** it, const char* end)
{
    int continuation = 0;

    switch (nr)
    {
        case __NR_sigaction:
            FOR_ARGUMENT(0, signal_name(value, it, end); return 1);
            break;

        case __NR_kill:
            FOR_ARGUMENT(1, signal_name(value, it, end); return 1);
            break;

        case __NR_ioctl:
            FOR_ARGUMENT(1,
                switch (value)
                {
                    FLAG(KDSETMODE);
                    FLAG(KDGETMODE);
                    FLAG(TCGETA);
                    FLAG(TCSETA);
                    FLAG(TIOCGETA);
                    FLAG(TIOCGWINSZ);
                    FLAG(TIOCSWINSZ);
                    FLAG(FBIOGET_VSCREENINFO);
                    FLAG(FBIOPUT_VSCREENINFO);
                    FLAG(FBIOGET_FSCREENINFO);
                });
            break;

        case __NR_open:
            FOR_ARGUMENT(1,
                BITFLAG_EMPTY(value & O_ACCMODE, O_RDONLY);
                BITFLAG(O_WRONLY);
                BITFLAG(O_RDWR);
                BITFLAG(O_CREAT);
                BITFLAG(O_EXCL);
                BITFLAG(O_NOCTTY);
                BITFLAG(O_TRUNC);
                BITFLAG(O_APPEND);
                BITFLAG(O_NONBLOCK);
                BITFLAG(O_NOFOLLOW);
                BITFLAG(O_CLOEXEC);
                BITFLAG(O_DIRECTORY);
                BITFLAG_LEFT();
                return 1);
            FOR_ARGUMENT(2,
                if (!(params[1] & O_CREAT))
                {
                    PRINT("0");
                    return 1;
                }
                BITFLAG_EMPTY(value, 0);
                BITFLAG(S_IRUSR);
                BITFLAG(S_IWUSR);
                BITFLAG(S_IXUSR);
                BITFLAG(S_IRGRP);
                BITFLAG(S_IWGRP);
                BITFLAG(S_IXGRP);
                BITFLAG(S_IROTH);
                BITFLAG(S_IWOTH);
                BITFLAG(S_IXOTH);
                return 1);
            break;

        case __NR_waitpid:
            FOR_ARGUMENT(2,
                BITFLAG_EMPTY(value, 0);
                BITFLAG(WNOHANG);
                BITFLAG(WUNTRACED);
                BITFLAG(WCONTINUED);
                BITFLAG_LEFT();
                return 1);
            break;

        case __NR_fcntl:
            FOR_ARGUMENT(1,
                switch (value)
                {
                    FLAG(F_DUPFD);
                    FLAG(F_GETFD);
                    FLAG(F_SETFD);
                    FLAG(F_GETFL);
                    FLAG(F_SETFL);
                }
                return 0);
            break;

        case __NR_mmap:
            FOR_ARGUMENT(2,
                BITFLAG_EMPTY(value, PROT_NONE);
                BITFLAG(PROT_READ);
                BITFLAG(PROT_WRITE);
                BITFLAG(PROT_EXEC);
                return 1);
            FOR_ARGUMENT(3,
                BITFLAG(MAP_SHARED);
                BITFLAG(MAP_PRIVATE);
                BITFLAG(MAP_TYPE);
                BITFLAG(MAP_FIXED);
                BITFLAG(MAP_ANONYMOUS);
                BITFLAG(MAP_GROWSDOWN);
                BITFLAG(MAP_DENYWRITE);
                BITFLAG(MAP_EXECUTABLE);
                BITFLAG(MAP_LOCKED);
                return 1);
            break;

        case __NR_mprotect:
            FOR_ARGUMENT(2,
                BITFLAG_EMPTY(value, PROT_NONE);
                BITFLAG(PROT_READ);
                BITFLAG(PROT_WRITE);
                BITFLAG(PROT_EXEC);
                return 1);
            break;

        case __NR_select:
            FOR_ARGUMENT(1, select_fds_print(params[0], ptr(value), it, end); return 1);
            FOR_ARGUMENT(2, select_fds_print(params[0], ptr(value), it, end); return 1);
            FOR_ARGUMENT(3, select_fds_print(params[0], ptr(value), it, end); return 1);
            FOR_ARGUMENT(4, timeval_print(ptr(value), it, end); return 1);
            break;

        case __NR_pselect:
            FOR_ARGUMENT(1, select_fds_print(params[0], ptr(value), it, end); return 1);
            FOR_ARGUMENT(2, select_fds_print(params[0], ptr(value), it, end); return 1);
            FOR_ARGUMENT(3, select_fds_print(params[0], ptr(value), it, end); return 1);
            FOR_ARGUMENT(4, timespec_print(ptr(value), it, end); return 1);
            break;

        case __NR_clock_gettime:
            FOR_ARGUMENT(0,
                switch (value)
                {
                    FLAG(CLOCK_REALTIME);
                    FLAG(CLOCK_MONOTONIC);
                    FLAG(CLOCK_MONOTONIC_COARSE);
                    FLAG(CLOCK_REALTIME_COARSE);
                });
            break;

        case __NR_clock_settime:
            FOR_ARGUMENT(0,
                switch (value)
                {
                    FLAG(CLOCK_REALTIME);
                    FLAG(CLOCK_MONOTONIC);
                    FLAG(CLOCK_MONOTONIC_COARSE);
                    FLAG(CLOCK_REALTIME_COARSE);
                });
            break;

        case __NR_timer_create:
            FOR_ARGUMENT(0,
                switch (value)
                {
                    FLAG(CLOCK_REALTIME);
                    FLAG(CLOCK_MONOTONIC);
                    FLAG(CLOCK_MONOTONIC_COARSE);
                    FLAG(CLOCK_REALTIME_COARSE);
                });
            break;
    }

    return 0;
}

int trace_syscall(unsigned long nr, ...)
{
    if (likely(!process_current->trace))
    {
        return 0;
    }

    const syscall_t* call = &trace_syscalls[nr];
    va_list args;
    char buf[256];
    char* it = buf;
    const char* end = buf + sizeof(buf);
    *it = 0;
    long* params;
    long params_buf[8];

    va_start(args, nr);
    params = parameters_get(params_buf, args, call->nargs);
    va_end(args);

    it = csnprintf(it, end, "%s(", call->name);

    for (size_t i = 0; i < call->nargs; ++i, ({ i < call->nargs ? it += snprintf(it, end - it, ", ") : 0; }))
    {
        type_t arg = call->args[i];
        long value = params[i];

        if (special_parameter(nr, i, value, params, &it, end))
        {
            continue;
        }

        int size_limit = nr == __NR_write
            ? params[2]
            : -1;

        switch (arg)
        {
            case TYPE_CONST_CHAR_PTR:
            {
                it += string_print(ptr(value), size_limit, it, end);
                break;
            }

            default:
            {
                const char* fmt = format_get(arg, value);
                it = csnprintf(it, end, fmt, value);
                break;
            }
        }
    }

    strcpy(it, ")");

    scoped_irq_lock();

    log_info("%s[%u]:   %s", process_current->name, process_current->pid, buf);

    if (process_current->trace & DTRACE_BACKTRACE)
    {
        const pt_regs_t* regs = ptr_get(addr(&nr));
        backtrace_user(KERN_INFO, regs, "\t");
    }

    return nr;
}

__attribute__((regparm(1))) void trace_syscall_end(int retval, unsigned long nr)
{
    char buf[128];
    char* it = buf;
    const char* end = buf + sizeof(buf);
    int errno;

    if (trace_syscalls[nr].ret == TYPE_VOID)
    {
        return;
    }

    scoped_irq_lock();

    if (process_current->trace & DTRACE_BACKTRACE)
    {
        it = csnprintf(it, end, "%s returned ", trace_syscalls[nr].name);
    }
    else
    {
        it = csnprintf(it, end, " = ");
    }

    if ((errno = errno_get(retval)))
    {
        it = csnprintf(it, end, "%d %s", retval, errno_name(errno));
    }
    else
    {
        it = csnprintf(it, end, format_get(trace_syscalls[nr].ret, retval), retval);
    }

    if (process_current->trace & DTRACE_BACKTRACE)
    {
        log_info("%s[%u]:   %s", process_current->name, process_current->pid, buf);
    }
    else
    {
        log_continue("%s", buf);
    }
}
