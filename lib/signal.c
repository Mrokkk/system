#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <kernel/syscall.h>

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

static void restore(void)
{
    syscall(__NR_sigreturn, 0);
}

void signals_init(void)
{
    struct sigaction s;
    s.sa_restorer = &restore;
    sigaction(0, &s, NULL);
}

LIBC_ALIAS(raise);
LIBC_ALIAS(sigprocmask);
LIBC_ALIAS(sigsuspend);
LIBC_ALIAS(sigemptyset);
LIBC_ALIAS(sigfillset);
LIBC_ALIAS(sigaddset);
LIBC_ALIAS(sigdelset);
LIBC_ALIAS(sigismember);
