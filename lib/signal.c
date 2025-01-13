#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <kernel/api/signal.h>
#include <kernel/api/syscall.h>

static void restore(void)
{
    syscall(__NR_sigreturn, 0);
}

int LIBC(raise)(int sig)
{
    return kill(getpid(), sig);
}

int LIBC(sigprocmask)(int how, const sigset_t* restrict set, sigset_t* restrict oset)
{
    UNUSED(how); UNUSED(set); UNUSED(oset);
    return 0;
}

int LIBC(sigsuspend)(const sigset_t* sigmask)
{
    UNUSED(sigmask);
    return 0;
}

int LIBC(sigemptyset)(sigset_t* set)
{
    UNUSED(set);
    return 0;
}

int LIBC(sigfillset)(sigset_t *set)
{
    UNUSED(set);
    memset(set, 0xff, sizeof(*set));
    return 0;
}

int LIBC(sigaddset)(sigset_t* set, int signum)
{
    UNUSED(set); UNUSED(signum);
    return 0;
}

int LIBC(sigdelset)(sigset_t* set, int signum)
{
    UNUSED(set); UNUSED(signum);
    return 0;
}

int LIBC(sigismember)(const sigset_t* set, int signum)
{
    UNUSED(set); UNUSED(signum);
    return 0;
}

int LIBC(sigaction)(int signum, const struct sigaction* act, struct sigaction* oldact);

int sigaction(int signum, const struct sigaction* act, struct sigaction* oldact)
{
    if (act)
    {
        struct sigaction* wact = (struct sigaction*)act;
        wact->sa_restorer = &restore;
    }
    return LIBC(sigaction)(signum, act, oldact);
}

int LIBC(signal)(int signum, sighandler_t handler)
{
    struct sigaction s = {
        .sa_flags = SA_RESTART,
        .sa_handler = handler,
        .sa_mask = 0,
        .sa_restorer = &restore,
    };
    return sigaction(signum, &s, NULL);
}

LIBC_ALIAS(raise);
LIBC_ALIAS(sigprocmask);
LIBC_ALIAS(sigsuspend);
LIBC_ALIAS(sigemptyset);
LIBC_ALIAS(sigfillset);
LIBC_ALIAS(sigaddset);
LIBC_ALIAS(sigdelset);
LIBC_ALIAS(sigismember);
LIBC_ALIAS(signal);
