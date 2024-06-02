#include "kernel/time.h"
#include <time.h>
#include <sys/time.h>

char* tzname[2];
static struct tm __tm;

int gettimeofday(struct timeval* restrict tp, void* restrict tzp)
{
    UNUSED(tp); UNUSED(tzp);
    tp->tv_sec = 0;
    tp->tv_usec = 0;
    return 0;
}

struct tm* localtime(const time_t* timep)
{
    UNUSED(timep);
    return &__tm;
}

time_t mktime(struct tm* tm)
{
    return mktime_raw(
        tm->tm_year,
        tm->tm_mon,
        tm->tm_mday,
        tm->tm_hour,
        tm->tm_min,
        tm->tm_sec);
}
