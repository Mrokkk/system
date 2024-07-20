#define log_fmt(fmt) "time: " fmt
#include <arch/io.h>
#include <kernel/time.h>
#include <kernel/clock.h>
#include <kernel/timer.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/seqlock.h>

volatile unsigned int jiffies;
static seqlock_t timestamp_lock;
timeval_t timestamp;
static uint32_t realtime;

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
    seqlock_write_lock(&timestamp_lock);
    timestamp.tv_usec += useconds;
    ts_normalize(&timestamp);
    seqlock_write_unlock(&timestamp_lock);
}

void timestamp_update(void)
{
    uint64_t cycles_cur, diff;

    scoped_irq_lock();

    cycles_cur = monotonic_clock->read();
    diff = (cycles_cur - monotonic_clock->cycles_prev) & monotonic_clock->mask;

    ts_update((diff * monotonic_clock->mult) >> monotonic_clock->shift);

    monotonic_clock->cycles_prev = cycles_cur;
}

void timestamp_get(timeval_t* ts)
{
    unsigned seq;

    do
    {
        seq = seqlock_read_begin(&timestamp_lock);
        __builtin_memcpy(ts, &timestamp, sizeof(*ts));
    }
    while (seqlock_read_check(&timestamp_lock, seq));
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

time_t sys_time(time_t* tloc)
{
    if (tloc && current_vm_verify(VERIFY_WRITE, tloc))
    {
        return -EFAULT;
    }

    return realtime + timestamp.tv_sec;
}

int sys_gettimeofday(struct timeval* tv, struct timezone* tz)
{
    if (tz)
    {
        if (unlikely(current_vm_verify(VERIFY_WRITE, tz)))
        {
            return -EFAULT;
        }
        tz->tz_dsttime = 0;
        tz->tz_minuteswest = 0;
    }

    unsigned seq;

    if (tv)
    {
        if (unlikely(current_vm_verify(VERIFY_WRITE, tv)))
        {
            return -EFAULT;
        }

        do
        {
            seq = seqlock_read_begin(&timestamp_lock);
            tv->tv_sec = realtime + timestamp.tv_sec;
            tv->tv_usec = timestamp.tv_usec;
        }
        while (seqlock_read_check(&timestamp_lock, seq));
    }

    return 0;
}

int sys_clock_gettime(clockid_t clockid, struct timespec* tp)
{
    int off = 0;

    if (unlikely(clockid >= CLOCK_ID_COUNT)) return -EINVAL;
    if (unlikely(current_vm_verify(VERIFY_WRITE, tp))) return -EFAULT;

    int coarse = clockid & __CLOCK_COARSE;

    if (!coarse)
    {
        timestamp_update();
    }

    unsigned seq;

    switch (clockid & __CLOCK_MASK)
    {
        case CLOCK_REALTIME:
            off = realtime;
            fallthrough;
        case CLOCK_MONOTONIC:
            do
            {
                seq = seqlock_read_begin(&timestamp_lock);
                tp->tv_sec = off + timestamp.tv_sec;
                tp->tv_nsec = timestamp.tv_usec * 1000;
            }
            while (seqlock_read_check(&timestamp_lock, seq));
            return 0;
    }

    return -EINVAL;
}

int sys_clock_settime(clockid_t clockid, const struct timespec* tp)
{
    if (unlikely(clockid >= CLOCK_ID_COUNT)) return -EINVAL;
    if (unlikely(current_vm_verify(VERIFY_READ, tp))) return -EFAULT;

    switch (clockid & __CLOCK_MASK)
    {
        case CLOCK_REALTIME:
        {
            timestamp_update();
            realtime = tp->tv_sec - timestamp.tv_sec;
            return 0;
        }
    }

    return -EINVAL;
}

void time_setup(void)
{
    read_overhead_measure();
    io_delay_measure();

    rtc_meas_t m;
    rtc_clock->read_rtc(&m);
    realtime = mktime(m.year, m.month, m.day, m.hour, m.minute, m.second);
}
