#include <time.h>
#include <sys/time.h>
#include <sys/utime.h>

long    timezone;
long    altzone;
char*   tzname[2];
int     daylight;
static struct tm __tm;

size_t strftime(
    char* s,
    size_t max,
    const char* restrict format,
    const struct tm* restrict tm)
{
    UNUSED(s); UNUSED(max); UNUSED(format); UNUSED(tm);
    return 0;
}

size_t strftime_l(
    char* s,
    size_t max,
    const char* restrict format,
    const struct tm* restrict tm,
    locale_t locale)
{
    UNUSED(s); UNUSED(max); UNUSED(format); UNUSED(tm); UNUSED(locale);
    return 0;
}

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

clock_t clock(void)
{
    return 0;
}

int utime(char const* pathname, const struct utimbuf* times)
{
    UNUSED(pathname); UNUSED(times);
    NOT_IMPLEMENTED(-1);
}
