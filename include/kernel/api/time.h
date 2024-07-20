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

struct timezone
{
    int tz_minuteswest; // minutes west of Greenwich
    int tz_dsttime;     // type of DST correction
};

struct itimerspec
{
    struct timespec it_interval;    // Interval for periodic timer
    struct timespec it_value;       // Initial expiration
};

#define __CLOCK_COARSE          (1 << 1)
#define __CLOCK_MASK            (1)

// Clock IDs
#define CLOCK_REALTIME          0
#define CLOCK_MONOTONIC         1
#define CLOCK_BOOTTIME          CLOCK_MONOTONIC
#define CLOCK_MONOTONIC_RAW     CLOCK_MONOTONIC
#define CLOCK_MONOTONIC_COARSE  (CLOCK_MONOTONIC | __CLOCK_COARSE)
#define CLOCK_REALTIME_COARSE   (CLOCK_REALTIME | __CLOCK_COARSE)
#define CLOCK_ID_COUNT          4

// Flags for timer_settime
#define TIMER_ABSTIME           1
#define TIMER_ADDREL            2
#define TIMER_AUTO_RELEASE      4
#define TIMER_PRESERVE_EXEC     8

int gettimeofday(
    struct timeval* restrict tv,
    struct timezone* restrict tz);

int clock_gettime(clockid_t clockid, struct timespec* tp);
int clock_settime(clockid_t clockid, const struct timespec* tp);

int timer_create(
    clockid_t clockid,
    struct sigevent* restrict evp,
    timer_t* restrict timerid);

int timer_settime(
    timer_t timerid,
    int flags,
    const struct itimerspec* restrict new_value,
    struct itimerspec* restrict old_value);
