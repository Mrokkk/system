#pragma once

#include <sys/poll.h>
#include <sys/types.h>
#include <kernel/api/time.h>

// https://pubs.opengroup.org/onlinepubs/007904975/basedefs/sys/time.h.html

#define ITIMER_REAL     1
#define ITIMER_VIRTUAL  2
#define ITIMER_PROF     3

struct itimerval
{
    struct timeval it_interval; // Timer interval
    struct timeval it_value;    // Current value
};

int getitimer(int which, struct itimerval* value);
int gettimeofday(struct timeval* restrict tp, void* restrict tzp);
int setitimer(
    int which,
    const struct itimerval* restrict value,
    struct itimerval* restrict ovalue);
