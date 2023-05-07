#pragma once

#include <stddef.h>
#include <arch/cpuid.h>

struct cpu_info
{
    char vendor[16];
    char producer[16];
    int family;
    int model;
    char name[49];
    int stepping;
    unsigned int mhz;
    unsigned int bogomips;
    unsigned int features[NR_FEATURES];
    unsigned int cacheline_size;
    unsigned int cache_size;
    char features_string[256];
    uint8_t lapic_id;
};

extern struct cpu_info cpu_info;
