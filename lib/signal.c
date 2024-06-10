#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <kernel/syscall.h>

int raise(int sig)
{
    return kill(getpid(), sig);
}

int sigprocmask(int how, const sigset_t* restrict set, sigset_t* restrict oset)
{
    UNUSED(how); UNUSED(set); UNUSED(oset);
    return 0;
}

int sigsuspend(const sigset_t* sigmask)
{
    UNUSED(sigmask);
    return 0;
}

int sigemptyset(sigset_t* set)
{
    UNUSED(set);
    return 0;
}

int sigfillset(sigset_t *set)
{
    UNUSED(set);
    memset(set, 0xff, sizeof(*set));
    return 0;
}

int sigaddset(sigset_t* set, int signum)
{
    UNUSED(set); UNUSED(signum);
    return 0;
}

int sigdelset(sigset_t* set, int signum)
{
    UNUSED(set); UNUSED(signum);
    return 0;
}

int sigismember(const sigset_t* set, int signum)
{
    UNUSED(set); UNUSED(signum);
    return 0;
}

static void restore(void)
{
    syscall(__NR_sigreturn, 0);
}

void signals_init()
{
    struct sigaction s;
    s.sa_restorer = &restore;
    sigaction(0, &s, NULL);
}
