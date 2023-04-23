#pragma once

#include <stdint.h>

#define HZ 100

#define UNIT_MHZ 1000000

typedef struct timestamp
{
    uint32_t seconds;
    uint32_t useconds;
} ts_t;

void delay_calibrate(void);
void timestamp_get(ts_t* ts);

static inline void ts_align(ts_t* ts)
{
    ts->seconds += ts->useconds / UNIT_MHZ;
    ts->useconds = ts->useconds % UNIT_MHZ;
}
