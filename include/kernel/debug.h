#pragma once

#if 1

#include <kernel/string.h>
#include <kernel/backtrace.h>

#define ASSERT(cond) \
    ({ \
        int a = (int)(cond); (void)a; \
        if (unlikely(!a)) \
        { \
            log_error("%s:%u: assertion "#cond" failed", __builtin_strrchr(__FILE__, '/') + 1, __LINE__); \
            backtrace_dump(log_error); \
        } \
        1; \
    })

#define ASSERT_NOT_REACHED() \
    do { \
        panic("Should not reach!"); \
    } while (1)

#else
#define ASSERT(cond) do { } while(0)
#define ASSERT_NOT_REACHED()
#endif
