#pragma once

#include <kernel/api/signal.h>

struct process;
typedef struct sigevent sigevent_t;

int do_kill(struct process* proc, int signum);
int signal_deliver(struct process* proc, int signum);
int signal_run(struct process* proc);
int sigaction(int signum, const struct sigaction* act, struct sigaction* oldact);

static inline int signum_exists(int s)
{
    return (s < NSIGNALS && s > 0);
}

static inline int signal_can_be_trapped(int s)
{
    return (s != SIGKILL && s != SIGSTOP);
}

const char* signame(int sig);
