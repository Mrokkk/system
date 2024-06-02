#pragma once

#include <kernel/signal.h>

typedef int sig_atomic_t;

#define SIG_BLOCK       0   // Block signals
#define SIG_UNBLOCK     1   // Unblock signals
#define SIG_SETMASK     2   // Set the set of blocked signals

int sigprocmask(int how, const sigset_t* restrict set, sigset_t* restrict oset);
int sigsuspend(const sigset_t* sigmask);

int sigemptyset(sigset_t* set);
int sigfillset(sigset_t *set);

int sigaddset(sigset_t* set, int signum);
int sigdelset(sigset_t* set, int signum);

int sigismember(const sigset_t* set, int signum);

int raise(int sig);
