#include <signal.h>
#include <stddef.h>
#include <unistd.h>
#include <kernel/syscall.h>

static void restore(void)
{
    syscall(__NR_sigreturn, 0);
}

int raise(int sig)
{
    return kill(getpid(), sig);
}

void signals_init()
{
    struct sigaction s;
    s.sa_restorer = &restore;
    sigaction(0, &s, NULL);
}
