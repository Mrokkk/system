#pragma once

#include <kernel/api/types.h>

#define HZ 100

#define CLOCK_REALTIME          1
#define CLOCK_MONOTONIC         2
#define CLOCK_MONOTONIC_RAW     3
#define CLOCK_REALTIME_COARSE   4
#define CLOCK_ID_COUNT          5

struct timeval
{
    time_t      tv_sec;
    suseconds_t tv_usec;
};
