#define log_fmt(fmt) "clock: " fmt
#include <kernel/abs.h>
#include <kernel/init.h>
#include <kernel/time.h>
#include <kernel/clock.h>
#include <kernel/kernel.h>

#define for_each_clock(c) \
    list_for_each_entry(c, &clocks, list_entry)

static uint64_t dummy_read(void);

static clock_source_t dummy_clock = {
    .name = "dummy",
    .read = &dummy_read,
};

static list_head_t clocks = LIST_INIT(clocks);

static clock_source_t* systick_clock;
static clock_source_t* monotonic_best;
clock_source_t* monotonic_clock = &dummy_clock;

static void clock_set(clock_source_t** current_best, clock_source_t* cs)
{
    *current_best = *current_best
        ? cs->freq_khz > (*current_best)->freq_khz
            ? cs
            : *current_best
        : cs;
}

#define HZ_SHIFT 20

static void clock_calc_mult_shift_hz(uint32_t* mult, uint32_t* shift, uint32_t from, uint32_t to)
{
    uint64_t tmp;
    tmp = (uint64_t)to << HZ_SHIFT;
    tmp += from / 2;
    do_div(tmp, from);
    *mult = tmp;
    *shift = HZ_SHIFT;
}

// This is from Linux, I will not pretend that I can do it better :)
static void clock_calc_mult_shift(uint32_t* mult, uint32_t* shift, uint32_t from, uint32_t to, uint32_t maxsec)
{
    uint64_t tmp;
    uint32_t sft, sftacc = 32;

    // Calculate the shift factor which is
    // limiting the conversion  range
    tmp = ((uint64_t)maxsec * from) >> 32;
    while (tmp)
    {
        tmp >>=1;
        sftacc--;
    }

    // Find the conversion shift/mult pair which has the
    // best accuracy and fits the maxsec conversion range
    for (sft = 32; sft > 0; sft--)
    {
        tmp = (uint64_t) to << sft;
        tmp += from / 2;
        do_div(tmp, from);
        if ((tmp >> sftacc) == 0)
        {
            break;
        }
    }

    *mult = tmp;
    *shift = sft;
}

#define MAX_UPDATE_LENGTH 5

void clock_monotonic_setup(clock_source_t* cs, uint32_t freq, uint32_t scale)
{
    if (scale == 1)
    {
        clock_calc_mult_shift_hz(&cs->mult, &cs->shift, freq, USEC_IN_SEC);
        clock_calc_mult_shift_hz(&cs->mult_ns, &cs->shift_ns, freq, NSEC_IN_SEC);
    }
    else
    {
        clock_calc_mult_shift(&cs->mult, &cs->shift, freq, USEC_IN_SEC, MAX_UPDATE_LENGTH * scale);
        clock_calc_mult_shift(&cs->mult_ns, &cs->shift_ns, freq, NSEC_IN_SEC, MAX_UPDATE_LENGTH * scale);
    }

    clock_set(&monotonic_best, cs);
}

UNMAP_AFTER_INIT int clock_source_register(clock_source_t* cs, uint32_t freq, uint32_t scale)
{
    list_init(&cs->list_entry);
    list_add_tail(&cs->list_entry, &clocks);

    if (cs->read)
    {
        clock_monotonic_setup(cs, freq, scale);
    }

    if (cs->enable_systick)
    {
        clock_set(&systick_clock, cs);
    }

    return 0;
}

UNMAP_AFTER_INIT static clock_source_t* clock_find(const char* name, int is_monotonic)
{
    clock_source_t* cs;

    if (!name)
    {
        return NULL;
    }

    for_each_clock(cs)
    {
        if (strcmp(name, cs->name))
        {
            continue;
        }

        if (is_monotonic && !cs->read)
        {
            log_warning("%s: not a monotonic-ready clock", name);
            return NULL;
        }
        else if (!is_monotonic && !cs->enable_systick)
        {
            log_warning("%s: not a systick-ready clock", name);
            return NULL;
        }

        return cs;
    }

    log_warning("%s: cannot find clock", name);

    return NULL;
}

UNMAP_AFTER_INIT int clock_sources_setup(void)
{
    int errno;
    clock_source_t* cs;
    clock_source_t* systick_override = clock_find(param_value_get(KERNEL_PARAM("systick")), 0);
    clock_source_t* monotonic_override = clock_find(param_value_get(KERNEL_PARAM("clock")), 1);

    if (systick_override)
    {
        log_info("overriding systick source as %s", systick_override->name);
        systick_clock = systick_override;
    }

    if (monotonic_override)
    {
        log_info("overriding monotonic source as %s", monotonic_override->name);
        monotonic_clock = monotonic_override;
    }
    else
    {
        monotonic_clock = monotonic_best;
    }

    panic_if(!systick_clock, "cannot select systick source");
    panic_if(!monotonic_clock, "cannot select monotonic clock");
    panic_if((errno = systick_clock->enable_systick()),
        "%s: cannot enable systick: %d", systick_clock->name, errno);

    panic_if(monotonic_clock->enable && (errno = monotonic_clock->enable()),
        "%s: cannot enable monotonic source: %d", monotonic_clock->name, errno);

    log_notice("available sources: ");

    for_each_clock(cs)
    {
        log_continue("%s ", cs->name);
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

    log_continue("; mult_ns = %x shift_ns = %u",
        monotonic_clock->mult_ns,
        monotonic_clock->shift_ns);

    return 0;
}

int clock_sources_shutdown(void)
{
    if (systick_clock && systick_clock->shutdown)
    {
        systick_clock->shutdown();
    }

    if (monotonic_clock && monotonic_clock != systick_clock && monotonic_clock->shutdown)
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
