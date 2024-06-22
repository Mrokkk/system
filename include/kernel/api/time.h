#pragma once

#include <kernel/api/types.h>

struct sigevent;

#define HZ 100

struct timeval
{
    time_t      tv_sec;  // Seconds
    suseconds_t tv_usec; // Microseconds
};

struct timespec
{
    time_t  tv_sec;      // Seconds
    long    tv_nsec;     // Nanoseconds
};

struct itimerspec
{
    struct timespec it_interval;    // Interval for periodic timer
    struct timespec it_value;       // Initial expiration
};

// Clock IDs
#define CLOCK_REALTIME          1
#define CLOCK_MONOTONIC         2
#define CLOCK_MONOTONIC_RAW     3
#define CLOCK_REALTIME_COARSE   4
#define CLOCK_ID_COUNT          5

// Flags for timer_settime
#define TIMER_ABSTIME           1
#define TIMER_ADDREL            2
#define TIMER_AUTO_RELEASE      4
#define TIMER_PRESERVE_EXEC     8

int timer_create(
    clockid_t clockid,
    struct sigevent* restrict evp,
    timer_t* restrict timerid);

int timer_settime(
    timer_t timerid,
    int flags,
    const struct itimerspec* restrict new_value,
    struct itimerspec* restrict old_value);
