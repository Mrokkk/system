#pragma once

#include <stdint.h>
#include <kernel/div.h>
#include <kernel/time.h>

struct clock_source;

typedef struct clock_source clock_source_t;

struct clock_source
{
    uint64_t (*read)(void);
    uint64_t prev_counter;
    uint32_t mult;
    uint32_t shift;
    uint64_t mask;

    const char* name;
    uint32_t freq_khz;
    int (*enable_systick)(void);
    int (*enable)(void);
    int (*shutdown)(void);
};

int clock_source_register(clock_source_t* cs);
int clock_sources_setup(void);
int clock_sources_shutdown(void);
