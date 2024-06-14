#include <time.h>
#include <sys/time.h>
#include <sys/utime.h>

long    timezone;
long    altzone;
char*   tzname[2];
int     daylight;
static struct tm __tm;

size_t LIBC(strftime)(
    char* s,
    size_t max,
    const char* restrict format,
    const struct tm* restrict tm)
{
    NOT_IMPLEMENTED(0, "%p, %u, \"%s\", %p", s, max, format, tm);
}

size_t LIBC(strftime_l)(
    char* s,
    size_t max,
    const char* restrict format,
    const struct tm* restrict tm,
    locale_t locale)
{
    NOT_IMPLEMENTED(0, "%p, %u, \"%s\", %p, %p", s, max, format, tm, locale);
}

int LIBC(gettimeofday)(struct timeval* restrict tp, void* restrict tzp)
{
    tp->tv_sec = 0;
    tp->tv_usec = 0;
    NOT_IMPLEMENTED(0, "%p, %p", tp, tzp);
}

struct tm* LIBC(localtime)(const time_t* timep)
{
    UNUSED(timep);
    return &__tm;
}

time_t LIBC(mktime)(struct tm* tm)
{
    return mktime_raw(
        tm->tm_year,
        tm->tm_mon,
        tm->tm_mday,
        tm->tm_hour,
        tm->tm_min,
        tm->tm_sec);
}

clock_t LIBC(clock)(void)
{
    return 0;
}

int LIBC(utime)(char const* pathname, const struct utimbuf* times)
{
    NOT_IMPLEMENTED(-1, "\"%s\", %p", pathname, times);
}

LIBC_ALIAS(strftime);
LIBC_ALIAS(strftime_l);
LIBC_ALIAS(gettimeofday);
LIBC_ALIAS(localtime);
LIBC_ALIAS(mktime);
LIBC_ALIAS(clock);
LIBC_ALIAS(utime);
