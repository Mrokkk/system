#pragma once

#define SIGHUP       1
#define SIGINT       2
#define SIGQUIT      3
#define SIGILL       4
#define SIGTRAP      5
#define SIGABRT      6
#define SIGIOT       6
#define SIGBUS       7
#define SIGFPE       8
#define SIGKILL      9
#define SIGUSR1     10
#define SIGSEGV     11
#define SIGUSR2     12
#define SIGPIPE     13
#define SIGALRM     14
#define SIGTERM     15
#define SIGSTKFLT   16
#define SIGCHLD     17
#define SIGCONT     18
#define SIGSTOP     19
#define SIGTSTP     20
#define SIGTTIN     21
#define SIGTTOU     22
#define SIGURG      23
#define SIGXCPU     24
#define SIGXFSZ     25
#define SIGVTALRM   26
#define SIGPROF     27
#define SIGWINCH    28
#define SIGIO       29
#define SIGPOLL     SIGIO
#define SIGPWR      30
#define SIGSYS      31
#define SIGUNUSED   31

#define NSIGNALS    32

#define SIG_ERR     ((void (*)(int))-1)
#define SIG_DFL     ((void (*)(int))0)
#define SIG_IGN     ((void (*)(int))1)
#define SIG_HOLD    ((void (*)(int))2)

typedef void (*sighandler_t)();
typedef void (*sigrestorer_t)(void);

struct process;

typedef unsigned long sigset_t;

struct sigaction
{
    void     (*sa_handler)(int);
    void     (*sa_sigaction)(int, void*, void*);
    sigset_t sa_mask;
    int      sa_flags;
    void     (*sa_restorer)(void);
};

int signal(int signum, sighandler_t handler);
int kill(int pid, int signum);
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
