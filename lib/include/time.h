#pragma once

#include <locale.h>
#include <kernel/time.h>

// https://pubs.opengroup.org/onlinepubs/009695399/basedefs/time.h.html

#define NSEC_PER_SEC    1000000000L // Nanoseconds per second
#define USEC_PER_SEC    1000000L    // Microseconds per second
#define NSEC_PER_USEC   1000L       // Nanoseconds per microsecond

struct timespec
{
    time_t  tv_sec;     // Seconds
    long    tv_nsec;    // Nanoseconds
};

struct tm
{
    int tm_sec;     // Seconds [0,60]
    int tm_min;     // Minutes [0,59]
    int tm_hour;    // Hour [0,23]
    int tm_mday;    // Day of month [1,31]
    int tm_mon;     // Month of year [0,11]
    int tm_year;    // Years since 1900
    int tm_wday;    // Day of week [0,6] (Sunday =0)
    int tm_yday;    // Day of year [0,365]
    int tm_isdst;   // Daylight Savings flag
};

size_t strftime(
    char* s,
    size_t max,
    const char* restrict format,
    const struct tm* restrict tm);

size_t strftime_l(
    char* s,
    size_t max,
    const char* restrict format,
    const struct tm* restrict tm,
    locale_t locale);

time_t time(time_t* time);

struct tm* localtime(const time_t* timep);
time_t mktime(struct tm* tm);

clock_t clock(void);
int clock_gettime(clockid_t clockid, struct timespec* tp);
int clock_settime(clockid_t clockid, struct timespec* tp);

extern long     timezone;
extern long     altzone;
extern char*    tzname[2];
extern int      daylight;
