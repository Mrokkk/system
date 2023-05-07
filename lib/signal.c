#include <signal.h>
#include <stddef.h>
#include <unistd.h>

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
