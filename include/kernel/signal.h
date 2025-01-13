#pragma once

#include <kernel/list.h>
#include <kernel/api/signal.h>

struct process;
typedef struct sigevent sigevent_t;
typedef struct sigaction sigaction_t;

struct signal
{
    siginfo_t   info;
    list_head_t list_entry;
};

typedef struct signal signal_t;

int do_kill(struct process* proc, siginfo_t* siginfo);
int signal_run(struct process* proc);

static inline int signum_exists(int s)
{
    return (s < NSIGNALS && s > 0);
}

static inline int signal_can_be_trapped(int s)
{
    return (s != SIGKILL && s != SIGSTOP);
}

const char* signame(int sig);
