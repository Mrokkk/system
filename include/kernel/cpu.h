#pragma once

#include <stddef.h>

struct cpu_cache
{
    const char* description;
    size_t size;
};

struct cpu_info
{
    char vendor[16];
    char producer[16];
    int family;
    int model;
    char name[46];
    int stepping;
    unsigned int mhz;
    unsigned int bogomips;
    unsigned int features;
    struct cpu_cache cache[3];
    struct cpu_cache instruction_cache;
};

extern struct cpu_info cpu_info;

size_t cpu_features_string_get(char* buffer);
