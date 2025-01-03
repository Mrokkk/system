#include <stdarg.h>
#include <kernel/ctype.h>
#include <kernel/trace.h>
#include <kernel/signal.h>
#include <kernel/kernel.h>
#include <kernel/minmax.h>
#include <kernel/process.h>
#include <kernel/process.h>
#include <kernel/api/mman.h>
#include <kernel/api/ioctl.h>
#include <kernel/api/types.h>
#include <kernel/api/unistd.h>
#include <kernel/api/syscall.h>

extern syscall_t trace_syscalls[];

#define ERRNO(x) [x] = #x

static const char* errors[] = {
    ERRNO(EPERM),
    ERRNO(ENOENT),
    ERRNO(ESRCH),
    ERRNO(EINTR),
    ERRNO(EIO),
    ERRNO(ENXIO),
    ERRNO(E2BIG),
    ERRNO(ENOEXEC),
    ERRNO(EBADF),
    ERRNO(ECHILD),
    ERRNO(EAGAIN),
    ERRNO(ENOMEM),
    ERRNO(EACCES),
    ERRNO(EFAULT),
    ERRNO(ENOTBLK),
    ERRNO(EBUSY),
    ERRNO(EEXIST),
    ERRNO(EXDEV),
    ERRNO(ENODEV),
    ERRNO(ENOTDIR),
    ERRNO(EISDIR),
    ERRNO(EINVAL),
    ERRNO(ENFILE),
    ERRNO(EMFILE),
    ERRNO(ENOTTY),
    ERRNO(ETXTBSY),
    ERRNO(EFBIG),
    ERRNO(ENOSPC),
    ERRNO(ESPIPE),
    ERRNO(EROFS),
    ERRNO(EMLINK),
    ERRNO(EPIPE),
    ERRNO(EDOM),
    ERRNO(ERANGE),
    ERRNO(EDEADLK),
    ERRNO(ENAMETOOLONG),
    ERRNO(ENOLCK),
    ERRNO(ENOSYS),
    ERRNO(ENOTEMPTY),
    ERRNO(ELOOP),
    ERRNO(ENOMSG),
    ERRNO(EIDRM),
    ERRNO(ECHRNG),
    ERRNO(EL2NSYNC),
    ERRNO(EL3HLT),
    ERRNO(EL3RST),
    ERRNO(ELNRNG),
    ERRNO(EUNATCH),
    ERRNO(ENOCSI),
    ERRNO(EL2HLT),
    ERRNO(EBADE),
    ERRNO(EBADR),
    ERRNO(EXFULL),
    ERRNO(ENOANO),
    ERRNO(EBADRQC),
    ERRNO(EBADSLT),
    ERRNO(EBFONT),
    ERRNO(ENOSTR),
    ERRNO(ENODATA),
    ERRNO(ETIME),
    ERRNO(ENOSR),
    ERRNO(ENONET),
    ERRNO(ENOPKG),
    ERRNO(EREMOTE),
    ERRNO(ENOLINK),
    ERRNO(EADV),
    ERRNO(ESRMNT),
    ERRNO(ECOMM),
    ERRNO(EPROTO),
    ERRNO(EMULTIHOP),
    ERRNO(EDOTDOT),
    ERRNO(EBADMSG),
    ERRNO(EOVERFLOW),
    ERRNO(ENOTUNIQ),
    ERRNO(EBADFD),
    ERRNO(EREMCHG),
    ERRNO(ELIBACC),
    ERRNO(ELIBBAD),
    ERRNO(ELIBSCN),
    ERRNO(ELIBMAX),
    ERRNO(ELIBEXEC),
    ERRNO(EILSEQ),
    ERRNO(ERESTART),
    ERRNO(ESTRPIPE),
    ERRNO(EUSERS),
    ERRNO(ENOTSOCK),
    ERRNO(EDESTADDRREQ),
    ERRNO(EMSGSIZE),
    ERRNO(EPROTOTYPE),
    ERRNO(ENOPROTOOPT),
    ERRNO(EPROTONOSUPPORT),
    ERRNO(ESOCKTNOSUPPORT),
    ERRNO(EOPNOTSUPP),
    ERRNO(EPFNOSUPPORT),
    ERRNO(EAFNOSUPPORT),
    ERRNO(EADDRINUSE),
    ERRNO(EADDRNOTAVAIL),
    ERRNO(ENETDOWN),
    ERRNO(ENETUNREACH),
    ERRNO(ENETRESET),
    ERRNO(ECONNABORTED),
    ERRNO(ECONNRESET),
    ERRNO(ENOBUFS),
    ERRNO(EISCONN),
    ERRNO(ENOTCONN),
    ERRNO(ESHUTDOWN),
    ERRNO(ETOOMANYREFS),
    ERRNO(ETIMEDOUT),
    ERRNO(ECONNREFUSED),
    ERRNO(EHOSTDOWN),
    ERRNO(EHOSTUNREACH),
    ERRNO(EALREADY),
    ERRNO(EINPROGRESS),
    ERRNO(ESTALE),
    ERRNO(EUCLEAN),
    ERRNO(ENOTNAM),
    ERRNO(ENAVAIL),
    ERRNO(EISNAM),
    ERRNO(EREMOTEIO),
    ERRNO(EDQUOT),
    ERRNO(ENOMEDIUM),
    ERRNO(EMEDIUMTYPE),
    ERRNO(ECANCELED),
    ERRNO(ENOKEY),
    ERRNO(EKEYEXPIRED),
    ERRNO(EKEYREVOKED),
    ERRNO(EKEYREJECTED),
    ERRNO(EOWNERDEAD),
    ERRNO(ENOTRECOVERABLE),
    ERRNO(ERFKILL),
    ERRNO(EHWPOISON),
};

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
        it = csnprintf(it, end, isprint(string[i]) ? "%c" : "\\%u", string[i]);
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
            return isprint(value) ? "\'%c\'" : "\'%x\'";
        case TYPE_SHORT:
        case TYPE_LONG: return "%d";
        case TYPE_CHAR_PTR:
        case TYPE_UNSIGNED_CHAR:
        case TYPE_UNSIGNED_SHORT:
        case TYPE_UNSIGNED_LONG:
        case TYPE_VOID_PTR:
            return "%x";
        case TYPE_VOID:
            return "void";
        default: return "unrecognized{%x}";
    }
}

static void* ptr_get(uintptr_t i)
{
    return ptr(i + sizeof(uintptr_t));
}

static int* parameters_get(int* buf, va_list args, size_t count)
{
    if (count > 5)
    {
        return va_arg(args, int*);
    }
    for (size_t i = 0; i < count; ++i)
    {
        buf[i] = va_arg(args, int);
    }
    return buf;
}

#define FLAG(f) \
    case f: *it = csnprintf(*it, end, #f); return 1

#define BITFLAG_EMPTY(v, c) \
    if (!(v)) \
    { \
        *it = csnprintf(*it, end, "%s", #c); \
        continuation = 1; \
    }

#define BITFLAG(f) \
    if (value & (f)) \
    { \
        if (continuation) \
        { \
            *it = csnprintf(*it, end, "|"); \
        } \
        *it = csnprintf(*it, end, #f); \
        value ^= f; \
        continuation = 1; \
    }

#define BITFLAG_LEFT() \
    if (value) \
    { \
        *it = csnprintf(*it, end, "%s%x", continuation ? "|" : "", value); \
    }

#define FOR_ARGUMENT(nr, ...) \
    do if (id == nr) \
    { \
        __VA_ARGS__; \
    } \
    while (0)

static int special_parameter(int nr, size_t id, int value, char** it, const char* end)
{
    int continuation = 0;
    switch (nr)
    {
        case __NR_signal:
        case __NR_sigaction:
            FOR_ARGUMENT(0,
                *it = csnprintf(*it, end, "%s", signame(value));
                return 1);
            break;

        case __NR_kill:
            FOR_ARGUMENT(1,
                *it = csnprintf(*it, end, "%s", signame(value));
                return 1);
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
    int* params;
    int params_buf[8];

    va_start(args, nr);
    params = parameters_get(params_buf, args, call->nargs);
    va_end(args);

    it = csnprintf(it, end, "%s(", call->name);

    for (size_t i = 0; i < call->nargs; ++i, ({ i < call->nargs ? it += snprintf(it, end - it, ", ") : 0; }))
    {
        type_t arg = call->args[i];
        int value = params[i];

        if (special_parameter(nr, i, value, &it, end))
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
        it = csnprintf(it, end, "%d %s", retval, errors[-errno]);
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
