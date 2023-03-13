#pragma once

#if 1

#include <kernel/string.h>
#include <kernel/backtrace.h>

#define ASSERT(cond) \
    do { \
        int a = (int)(cond); (void)a; \
        if (!a) \
        { \
            log_error("assertion "#cond" failed"); \
            backtrace_dump(); \
        } \
    } while (0)

#define ASSERT_NOT_REACHED() \
    do { \
        panic("Should not reach!"); \
    } while (1)

#else
#define ASSERT(cond) do { } while(0)
#define ASSERT_NOT_REACHED()
#endif
