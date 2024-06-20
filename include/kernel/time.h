#pragma once

#include <stdint.h>
#include <kernel/clock.h>
#include <kernel/api/time.h>
#include <kernel/api/types.h>

#define KHz             1000
#define MHz             (1000 * KHz)
#define GHz             (1000 * MHz)
#define FREQ_100HZ      100

#define NSEC_IN_SEC     GHz
#define USEC_IN_SEC     MHz
#define USEC_IN_MSEC    KHz
#define NSEC_IN_MSEC    MHz

typedef struct timeval timeval_t;
typedef struct timespec timespec_t;

void timestamp_get(timeval_t* ts);
void timestamp_update(void);
void udelay(uint32_t useconds);
void mdelay(uint32_t mseconds);
uint64_t cycles2us(uint64_t cycles);
uint64_t cycles2ns(uint64_t cycles);

time_t mktime(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t hour,
    uint32_t minute,
    uint32_t second);

void time_setup(void);

#define MEASURE_OPERATION(unit, ...) \
    ({ \
        uint64_t __s = monotonic_clock->read(); \
        mb(); \
        __VA_ARGS__; \
        mb(); \
        uint64_t __e = monotonic_clock->read(); \
        cycles2##unit(((__e - __s) & monotonic_clock->mask) - monotonic_clock->read_overhead); \
    })
