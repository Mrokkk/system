#pragma once

#include <sys/time.h>
#include <sys/types.h>

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_resource.h.html

typedef unsigned long rlim_t;

#define PRIO_PROCESS    0
#define PRIO_PGRP       1
#define PRIO_USER       2

#define RLIM_INFINITY   ((rlim_t)-1)
#define RLIM_SAVED_MAX  ((rlim_t)-2)
#define RLIM_SAVED_CUR  ((rlim_t)-3)

#define RUSAGE_SELF     0
#define RUSAGE_CHILDREN 1

#define RLIMIT_CORE     0
#define RLIMIT_CPU      1
#define RLIMIT_DATA     2
#define RLIMIT_FSIZE    3
#define RLIMIT_NOFILE   4
#define RLIMIT_STACK    5
#define RLIMIT_AS       6

struct rlimit
{
    rlim_t rlim_cur;    // the current (soft) limit
    rlim_t rlim_max;    // the hard limit
};

struct rusage
{
    struct timeval ru_utime; // user time used
    struct timeval ru_stime; // system time used
};

int getpriority(int which, id_t who);
int getrlimit(int resource, struct rlimit* rlp);
int getrusage(int who, struct rusage* r_usage);
int setpriority(int which, id_t who, int value);
int setrlimit(int resource, const struct rlimit* rlp);
