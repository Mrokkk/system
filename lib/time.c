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

struct tm* LIBC(gmtime)(const time_t* timep)
{
    NOT_IMPLEMENTED(&__tm, "%p", timep);
}

struct tm* LIBC(localtime)(const time_t* timep)
{
    NOT_IMPLEMENTED(&__tm, "%p", timep);
}

static inline time_t mktime_raw(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t hour,
    uint32_t minute,
    uint32_t second)
{
    if (0 >= (int)(month -= 2))
    {   // 1..12 -> 11,12,1..10
        month += 12; // put Feb last since it has a leap day
        year -= 1;
    }

    return (((year / 4 - year / 100 + year / 400 + 367 * month / 12 + day + year * 365 - 719499 // days
          ) * 24 + hour // hours
        ) * 60 + minute // minutes
      ) * 60 + second; // seconds
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

void LIBC(tzset)(void)
{
}

char* LIBC(ctime)(const time_t* timep)
{
    NOT_IMPLEMENTED(NULL, "%p", timep);
}

double LIBC(difftime)(time_t time1, time_t time0)
{
    return (double)(time1 - time0);
}

LIBC_ALIAS(strftime);
LIBC_ALIAS(strftime_l);
LIBC_ALIAS(gettimeofday);
LIBC_ALIAS(gmtime);
LIBC_ALIAS(localtime);
LIBC_ALIAS(mktime);
LIBC_ALIAS(clock);
LIBC_ALIAS(utime);
LIBC_ALIAS(tzset);
LIBC_ALIAS(ctime);
LIBC_ALIAS(difftime);
