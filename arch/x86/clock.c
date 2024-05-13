#include <kernel/time.h>
#include <kernel/clock.h>
#include <kernel/kernel.h>

#define CLOCK_SOURCES_COUNT 4
#define TEST_VALUE 2938498ULL

static uint64_t dummy_read(void);

static clock_source_t dummy_clock = {
    .name = "dummy",
    .read = &dummy_read,
};

static clock_source_t* clock_sources[CLOCK_SOURCES_COUNT];
static clock_source_t* systick_clock;
static clock_source_t* monotonic_clock = &dummy_clock;

volatile unsigned int jiffies;
static ts_t timestamp;

int clock_source_register(clock_source_t* cs)
{
    for (int i = 0; i < CLOCK_SOURCES_COUNT; ++i)
    {
        if (!clock_sources[i])
        {
            clock_sources[i] = cs;
            return 0;
        }
    }
    return -EBUSY;
}

void timestamp_update(void)
{
    flags_t flags;
    uint64_t counter, diff;

    irq_save(flags);

    counter = monotonic_clock->read();
    diff = (counter - monotonic_clock->prev_counter) & monotonic_clock->mask;
    timestamp.useconds += (diff * monotonic_clock->mult) >> monotonic_clock->shift;
    ts_align(&timestamp);
    monotonic_clock->prev_counter = counter;

    irq_restore(flags);
}

void timestamp_get(ts_t* ts)
{
    memcpy(ts, &timestamp, sizeof(*ts));
}

static inline void clock_set(clock_source_t** current_best, clock_source_t* cs)
{
    *current_best = *current_best
        ? cs->freq_khz > (*current_best)->freq_khz
            ? cs
            : *current_best
        : cs;
}

static inline void mult_shift_calculate(uint32_t* mult, uint32_t* shift, uint32_t from)
{
    uint64_t tmp = (uint64_t)USEC_IN_MSEC << 32;
    tmp += from / 2;
    do_div(tmp, from);

    *mult = tmp;
    *shift = 32;
}

void udelay(uint32_t useconds)
{
    uint64_t current = monotonic_clock->read();
    uint64_t diff = (uint64_t)useconds * (uint64_t)monotonic_clock->freq_khz;
    do_div(diff, 1000);
    uint64_t next = current + diff;
    while (monotonic_clock->read() < next);
}

void mdelay(uint32_t mseconds)
{
    uint64_t current = monotonic_clock->read();
    uint64_t next = current + (uint64_t)mseconds * (uint64_t)monotonic_clock->freq_khz;
    if (unlikely(next > monotonic_clock->mask))
    {
        return;
    }
    while ((current = monotonic_clock->read()) < next);
}

UNMAP_AFTER_INIT int clock_sources_setup(void)
{
    int errno;
    clock_source_t* systick_best = NULL;
    clock_source_t* monotonic_best = NULL;

    for (int i = 0; i < 4; ++i)
    {
        if (!clock_sources[i])
        {
            continue;
        }

        if (clock_sources[i]->enable_systick)
        {
#if 0
            if (!strcmp("i8253", clock_sources[i]->name))
            {
                systick_best = clock_sources[i];
            }
#else
            clock_set(&systick_best, clock_sources[i]);
#endif
        }

        if (clock_sources[i]->read)
        {
#if 0
            if (!strcmp("i8253", clock_sources[i]->name))
            {
                monotonic_best = clock_sources[i];
            }
#else
            clock_set(&monotonic_best, clock_sources[i]);
#endif
        }
    }

    systick_clock = systick_best;
    monotonic_clock = monotonic_best;

    if (unlikely(!systick_clock))
    {
        panic("no systick source!");
    }

    if (unlikely(!monotonic_clock))
    {
        panic("no monotonic clock source!");
    }

    if ((errno = systick_clock->enable_systick()))
    {
        panic("cannot enable systick on source %s: %d", systick_clock->name, errno);
    }

    mult_shift_calculate(&monotonic_clock->mult, &monotonic_clock->shift, monotonic_clock->freq_khz);

    if (monotonic_clock->enable && (errno = monotonic_clock->enable()))
    {
        panic("cannot enable monotonic source %s: %d", monotonic_clock->name, errno);
    }

    log_notice("systick source:   %- 10s freq: %u.%03u MHz",
        systick_clock->name,
        systick_clock->freq_khz / 1000,
        systick_clock->freq_khz % 1000);

    log_notice("monotonic source: %- 10s freq: %u.%03u MHz; mult = %x shift = %u",
        monotonic_clock->name,
        monotonic_clock->freq_khz / 1000,
        monotonic_clock->freq_khz % 1000,
        monotonic_clock->mult,
        monotonic_clock->shift);

    uint64_t test = (TEST_VALUE * monotonic_clock->mult) >> monotonic_clock->shift;
    uint64_t test2 = ((TEST_VALUE * 1000) / (monotonic_clock->freq_khz));

    ASSERT(test == test2);

    return 0;
}

int clock_sources_shutdown(void)
{
    if (systick_clock && systick_clock->shutdown)
    {
        systick_clock->shutdown();
    }
    if (monotonic_clock != systick_clock && monotonic_clock->shutdown)
    {
        monotonic_clock->shutdown();
    }
    monotonic_clock = &dummy_clock;
    return 0;
}

static uint64_t dummy_read(void)
{
    return 0;
}
