#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/utime.h>

long    timezone;
long    altzone;
char*   tzname[2];
int     daylight;
static struct tm __tm;

struct name
{
    const char* abbrv;
    const char* full;
};

static const struct name month_names[] = {
    {"Jan", "January"},
    {"Feb", "February"},
    {"Mar", "March"},
    {"Apr", "April"},
    {"May", "May"},
    {"Jun", "June"},
    {"Jul", "July"},
    {"Aug", "August"},
    {"Sep", "September"},
    {"Oct", "October"},
    {"Nov", "November"},
    {"Dec", "December"}
};

static const struct name day_names[] = {
    {"Sun", "Sunday"},
    {"Mon", "Monday"},
    {"Tue", "Tuesday"},
    {"Wed", "Wednesday"},
    {"Thu", "Thursday"},
    {"Fri", "Friday"},
    {"Sat", "Saturday"},
};

#define PRINT(...) \
    ({ int res = snprintf(s, max, __VA_ARGS__); max -= res; res; })

#define EQUIVALENT(fmt) \
    ({ int res = LIBC(strftime)(s, max, fmt, tm); max -= res; res; })

size_t LIBC(strftime)(
    char* s,
    size_t max,
    const char* restrict format,
    const struct tm* restrict tm)
{
    const char* const orig_s = s;

    while (*format)
    {
        switch (*format)
        {
            case '%':
                ++format;
                switch (*format)
                {
                    case 'a':
                        s += PRINT(day_names[tm->tm_wday].abbrv);
                        break;
                    case 'A':
                        s += PRINT(day_names[tm->tm_wday].full);
                        break;
                    case 'b':
                    case 'h':
                        s += PRINT(month_names[tm->tm_mon].abbrv);
                        break;
                    case 'B':
                        s += PRINT(month_names[tm->tm_mon].full);
                        break;
                    case 'c':
                        s += EQUIVALENT("%a %b %e %H:%M:%S %Y");
                        break;
                    case 'C':
                        s += PRINT("%02u", tm->tm_year / 100);
                        break;
                    case 'd':
                        s += PRINT("%02u", tm->tm_mday);
                        break;
                    case 'D':
                        s += EQUIVALENT("%m/%d/%y");
                        break;
                    case 'e':
                        s += PRINT("% 2u", tm->tm_mday);
                        break;
                    case 'F':
                        s += EQUIVALENT("%Y-%m-%d");
                        break;
                    case 'H':
                        s += PRINT("%02u", tm->tm_hour);
                        break;
                    case 'j':
                        s += PRINT("%03u", tm->tm_yday + 1);
                        break;
                    case 'k':
                        s += PRINT("% 2u", tm->tm_hour);
                        break;
                    case 'm':
                        s += PRINT("%02u", tm->tm_mon + 1);
                        break;
                    case 'M':
                        s += PRINT("%02u", tm->tm_min);
                        break;
                    case 'n':
                        s += PRINT("\n");
                        break;
                    case 's':
                        s += PRINT("%u", mktime((struct tm*)tm));
                        break;
                    case 'S':
                        s += PRINT("%02u", tm->tm_sec);
                        break;
                    case 't':
                        s += PRINT("\t");
                        break;
                    case 'T':
                    case 'X':
                        s += EQUIVALENT("%H:%M:%S");
                        break;
                    case 'x':
                        s += EQUIVALENT("%m/%d/%y");
                        break;
                    case 'y':
                        s += PRINT("%02u", (tm->tm_year + 1900) % 100);
                        break;
                    case 'Y':
                        s += PRINT("%04u", tm->tm_year + 1900);
                        break;
                    case 'z':
                        s += PRINT("+0000");
                        break;
                    case 'Z':
                        s += PRINT("UTC");
                        break;
                    case '%':
                        s += PRINT("%%");
                        break;
                    default:
                        break;
                }
                break;
            default:
                *s++ = *format;
                break;
        }
        ++format;
    }
    return s - orig_s;
}

size_t LIBC(strftime_l)(
    char* s,
    size_t max,
    const char* restrict format,
    const struct tm* restrict tm,
    locale_t locale)
{
    UNUSED(locale);
    return strftime(s, max, format, tm);
}

char* LIBC(asctime)(const struct tm* tm)
{
    static char buf[32];
    strftime(buf, 32, "%a %b %e %H:%M:%S %Y\n", tm);
    return buf;
}

char* LIBC(ctime)(const time_t* timep)
{
    return asctime(localtime(timep));
}

#define SECONDS_PER_DAY (60 * 60 * 24)

static inline int is_leap(int year)
{
    return year % 4 == 0 && (year % 100 || year % 400 == 0);
}

static int days_in_year(int year)
{
    return 365 + (is_leap(year) ? 1 : 0);
}

static int days_in_month(int year, int month)
{
    static int days_in_months[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 1)
    {
        return is_leap(year) ? 29 : 28;
    }

    return days_in_months[month];
}

static int day_of_week(int year, int month, int day)
{
    static int month_start[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

    if (month < 2)
    {
        --year;
    }

    return (year + year / 4 - year / 100 + year / 400 + month_start[month] + day) % 7;
}

static void time_to_tm(const time_t time, struct tm* tm)
{
    int seconds = time;
    int days, month, year = 1970;

    for (; seconds >= days_in_year(year) * SECONDS_PER_DAY; ++year)
    {
        seconds -= days_in_year(year) * SECONDS_PER_DAY;
    }
    for (; seconds < 0; --year)
    {
        seconds += days_in_year(year - 1) * SECONDS_PER_DAY;
    }

    days = seconds / SECONDS_PER_DAY;
    seconds = seconds % SECONDS_PER_DAY;

    for (month = 0; month < 11 && days >= days_in_month(year, month); ++month)
    {
        days -= days_in_month(year, month);
    }

    tm->tm_yday = days;
    tm->tm_year = year - 1900;
    tm->tm_sec = seconds % 60;
    seconds /= 60;
    tm->tm_min = seconds % 60;
    tm->tm_hour = seconds / 60;
    tm->tm_mday = days + 1;
    tm->tm_wday = day_of_week(year, month, tm->tm_mday);
    tm->tm_mon = month;
    tm->tm_isdst = 0;
}

struct tm* LIBC(localtime)(const time_t* timep)
{
    time_to_tm(*timep, &__tm);
    return &__tm;
}

struct tm* LIBC(localtime_r)(const time_t* timep, struct tm* result)
{
    time_to_tm(*timep, result);
    return result;
}

struct tm* LIBC(gmtime)(const time_t* timep)
{
    time_to_tm(*timep, &__tm);
    return &__tm;
}

struct tm* LIBC(gmtime_r)(const time_t* timep, struct tm* result)
{
    time_to_tm(*timep, result);
    return result;
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

double LIBC(difftime)(time_t time1, time_t time0)
{
    return (double)(time1 - time0);
}

LIBC_ALIAS(asctime);
LIBC_ALIAS(strftime);
LIBC_ALIAS(strftime_l);
LIBC_ALIAS(localtime);
LIBC_ALIAS(localtime_r);
LIBC_ALIAS(gmtime);
LIBC_ALIAS(gmtime_r);
LIBC_ALIAS(mktime);
LIBC_ALIAS(clock);
LIBC_ALIAS(utime);
LIBC_ALIAS(tzset);
LIBC_ALIAS(ctime);
LIBC_ALIAS(difftime);
