#define log_fmt(fmt) "time: " fmt
#include <arch/io.h>
#include <arch/rtc.h>
#include <kernel/time.h>
#include <kernel/clock.h>
#include <kernel/timer.h>
#include <kernel/kernel.h>

volatile unsigned int jiffies;
timeval_t timestamp;

// This is from Linux source code
time_t mktime(
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

static void ts_update(uint64_t useconds)
{
    timestamp.tv_usec += useconds;
    ts_normalize(&timestamp);
}

void timestamp_update(void)
{
    flags_t flags;
    uint64_t cycles_cur, diff;

    irq_save(flags);

    cycles_cur = monotonic_clock->read();
    diff = (cycles_cur - monotonic_clock->cycles_prev) & monotonic_clock->mask;

    ts_update((diff * monotonic_clock->mult) >> monotonic_clock->shift);

    monotonic_clock->cycles_prev = cycles_cur;

    irq_restore(flags);
}

void timestamp_get(timeval_t* ts)
{
    memcpy(ts, &timestamp, sizeof(*ts));
}

// FIXME: delay is still broken with i8253
static inline void wait_for(uint64_t time)
{
    uint64_t current = monotonic_clock->read();
    uint64_t elapsed = 0;
    uint64_t next;
    do
    {
        elapsed += ((next = monotonic_clock->read()) - current) & monotonic_clock->mask;
        current = next;
    }
    while (elapsed < time);
}

void udelay(uint32_t useconds)
{
    uint64_t diff = (uint64_t)useconds * (uint64_t)monotonic_clock->freq_khz;
    do_div(diff, 1000);
    wait_for(diff);
}

void mdelay(uint32_t mseconds)
{
    uint64_t diff = (uint64_t)mseconds * (uint64_t)monotonic_clock->freq_khz;
    wait_for(diff);
}

uint64_t cycles2us(uint64_t cycles)
{
    return ((cycles & monotonic_clock->mask) * monotonic_clock->mult) >> monotonic_clock->shift;
}

uint64_t cycles2ns(uint64_t cycles)
{
    return ((cycles & monotonic_clock->mask) * monotonic_clock->mult_ns) >> monotonic_clock->shift_ns;
}

static void read_overhead_measure(void)
{
    uint64_t start = monotonic_clock->read();
    uint64_t stop = monotonic_clock->read();
    monotonic_clock->read_overhead = (uint32_t)(stop - start);
    log_info("read overhead: %u ns", cycles2ns(monotonic_clock->read_overhead));
}

#define IO_LOOPS 1000

static void io_delay_measure(void)
{
    uint32_t nseconds = MEASURE_OPERATION(ns,
        for (int i = 0; i < IO_LOOPS; ++i)
        {
            io_dummy();
        });

    log_info("dummy IO duration: %u ns", nseconds / IO_LOOPS);
}

void time_setup(void)
{
    read_overhead_measure();
    io_delay_measure();
}
