#include <string.h>
#include <signal.h>

struct sig
{
    const char* name;
    const char* desc;
};

#define SIG(N, D) \
    [SIG##N] = {.name = #N, .desc = D}

static struct sig siglist[] = {
    [0] = {NULL, NULL},
    SIG(HUP, "Hangup"),
    SIG(INT, "Interrupt"),
    SIG(QUIT, "Quit"),
    SIG(ILL, "Illegal instruction"),
    SIG(TRAP, "Trace/breakpoint trap"),
    SIG(ABRT, "Aborted"),
    SIG(BUS, "Bus error"),
    SIG(FPE, "Floating point exception"),
    SIG(KILL, "Killed"),
    SIG(USR1, "User defined signal 1"),
    SIG(SEGV, "Segmentation fault"),
    SIG(USR2, "User defined signal 2"),
    SIG(PIPE, "Broken pipe"),
    SIG(ALRM, "Alarm clock"),
    SIG(TERM, "Terminated"),
    SIG(STKFLT, "Stack fault"),
    SIG(CHLD, "Child exited"),
    SIG(CONT, "Continued"),
    SIG(STOP, "Stopped (signal)"),
    SIG(TSTP, "Stopped"),
    SIG(TTIN, "Stopped (tty input)"),
    SIG(TTOU, "Stopped (tty output)"),
    SIG(URG, "Urgent I/O condition"),
    SIG(XCPU, "CPU time limit exceeded"),
    SIG(XFSZ, "File size limit exceeded"),
    SIG(VTALRM, "Virtual timer expired"),
    SIG(PROF, "Profiling timer expired"),
    SIG(WINCH, "Window changed"),
    SIG(POLL, "I/O possible"),
    SIG(PWR, "Power failure"),
    SIG(SYS, "Bad system call"),
};

char* LIBC(strsignal)(int sig)
{
    if (UNLIKELY((unsigned)sig >= NSIGNALS))
    {
        return NULL;
    }

    return (char*)siglist[sig].desc;
}

const char* LIBC(sigdescr_np)(int sig)
{
    return strsignal(sig);
}

const char* LIBC(sigabbrev_np)(int sig)
{
    if (UNLIKELY((unsigned)sig >= NSIGNALS))
    {
        return NULL;
    }

    return siglist[sig].name;
}

LIBC_ALIAS(strsignal);
LIBC_ALIAS(sigdescr_np);
LIBC_ALIAS(sigabbrev_np);
