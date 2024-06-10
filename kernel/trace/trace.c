#include <stdarg.h>
#include <kernel/ctype.h>
#include <kernel/trace.h>
#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/syscall.h>

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

static const char* format_get(type_t t, int value)
{
    switch (t)
    {
        case TYPE_CHAR:
            return isprint(value) ? "\'%c\'" : "\'%x\'";
        case TYPE_SHORT:
        case TYPE_LONG: return "%d";
        case TYPE_CHAR_PTR:
            if (!vm_verify(VERIFY_READ, (void*)value, 1, process_current->mm->vm_areas))
            {
                return "\"%s\"";
            }
            fallthrough;
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

static void* ptr_get(uint32_t i)
{
    return ptr(i + sizeof(uint32_t));
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
    *it = 0;

    it += sprintf(it, "%s(", call->name);

    va_start(args, nr);

    for (size_t i = 0; i < call->nargs; ++i)
    {
        type_t arg = call->args[i];
        int value = va_arg(args, int);
        const char* fmt = format_get(arg, value);
        it += sprintf(it, fmt, value);
        if (i + 1 < call->nargs)
        {
            it += sprintf(it, ", ");
        }
    }

    va_end(args);

    strcpy(it, ")");

    log_info("%s[%u]:   %s", process_current->name, process_current->pid, buf);

    if (DEBUG_BTUSER)
    {
        const pt_regs_t* regs = ptr_get(addr(&nr));
        backtrace_user(log_info, regs, page_virt_ptr(process_current->bin->symbols_pages), "\t");
    }

    return nr;
}

__attribute__((regparm(1))) void trace_syscall_end(int ret, unsigned long nr)
{
    char buf[128];
    char* it = buf;

    if (DEBUG_BTUSER)
    {
        it += sprintf(it, "%s returned ", trace_syscalls[nr].name);
    }
    else
    {
        it += sprintf(it, " = ");
    }
    if (ret < 0)
    {
        it += sprintf(it, "%d %s", ret, errors[-ret]);
    }
    else
    {
        it += sprintf(it, format_get(trace_syscalls[nr].ret, ret), ret);
    }
    if (DEBUG_BTUSER)
    {
        log_info("%s[%u]:   %s", process_current->name, process_current->pid, buf);
    }
    else
    {
        log_continue("%s", buf);
    }
}
