#pragma once

#include <stddef.h>
#include <arch/cpuid.h>

struct cpu_info
{
    char         vendor[16];
    int          vendor_id;
    char         producer[16];
    int          family;
    int          model;
    unsigned int max_function;
    unsigned int max_extended_function;
    int          type;
    char         name[49];
    int          stepping;
    unsigned int phys_bits;
    unsigned int virt_bits;
    unsigned int features[NR_FEATURES];
    unsigned int cacheline_size;
    unsigned int cache_size;
    uint8_t      lapic_id;
};

typedef struct cpu_info cpu_info_t;

extern struct cpu_info cpu_info;
