#pragma once

#include <stdint.h>
#include <kernel/types.h>

#define HZ 100

#define KHz             1000
#define MHz             (1000 * KHz)
#define GHz             (1000 * MHz)
#define FREQ_100HZ      100

#define NSEC_IN_SEC     GHz
#define USEC_IN_SEC     MHz
#define USEC_IN_MSEC    KHz

struct timestamp
{
    uint32_t seconds;
    uint32_t useconds;
};

typedef struct timestamp ts_t;

void timestamp_get(ts_t* ts);
void timestamp_update(void);
void udelay(uint32_t useconds);
void mdelay(uint32_t mseconds);

static inline void ts_align(ts_t* ts)
{
    ts->seconds += ts->useconds / MHz;
    ts->useconds = ts->useconds % MHz;
}

// This one is from Linux 2.0
static inline uint32_t mktime(
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
