#pragma once

#include <stdint.h>
#include <kernel/div.h>
#include <kernel/rtc.h>
#include <kernel/list.h>
#include <kernel/time.h>
#include <arch/processor.h>

struct clock_source;

typedef struct clock_source CACHELINE_ALIGN clock_source_t;

struct clock_source
{
    uint64_t (*read)(void);
    uint64_t cycles_prev;
    uint32_t mult;
    uint32_t shift;
    uint64_t mask;

    uint32_t mult_ns;
    uint32_t shift_ns;
    uint32_t read_overhead;

    void (*read_rtc)(rtc_meas_t*);

    const char* name;
    uint32_t freq_khz;

    list_head_t list_entry;

    int (*enable_systick)(void);
    int (*enable)(void);
    int (*shutdown)(void);
};

int clock_source_register(clock_source_t* cs, uint32_t freq, uint32_t scale);
int clock_sources_setup(void);
int clock_sources_shutdown(void);

static inline int clock_source_register_hz(clock_source_t* cs, uint32_t freq)
{
    return clock_source_register(cs, freq, 1);
}

static inline int clock_source_register_khz(clock_source_t* cs, uint32_t freq)
{
    return clock_source_register(cs, freq, 1000);
}

extern clock_source_t* rtc_clock;
extern clock_source_t* monotonic_clock;
