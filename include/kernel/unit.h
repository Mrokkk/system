#pragma once

#define KiB (1024UL)
#define MiB (1024UL * KiB)
#define GiB (1024UL * MiB)

#define human_size(size, unit) \
    ({ \
        unit = "B"; \
        if (size >= GiB) \
        { \
            do_div(size, GiB); \
            unit = "GiB"; \
        } \
        else if (size >= MiB) \
        { \
            do_div(size, MiB); \
            unit = "MiB"; \
        } \
        else if (size >= 2 * KiB) \
        { \
            do_div(size, KiB); \
            unit = "KiB"; \
        } \
    })
