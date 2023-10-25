#pragma once

#include <kernel/string.h>
#include <kernel/backtrace.h>

#define DEBUG_VM            0
#define DEBUG_VM_APPLY      0
#define DEBUG_VM_COPY       0
#define DEBUG_VM_COW        0

#define DEBUG_WAIT_QUEUE    0
#define DEBUG_PAGE          0
#define DEBUG_PAGE_DETAILED 0 // Collects page_alloc caller address
#define DEBUG_KMALLOC       0
#define DEBUG_FMALLOC       0
#define DEBUG_PROCESS       0
#define DEBUG_EXIT          0
#define DEBUG_SIGNAL        0
#define DEBUG_POLL          0
#define DEBUG_EXCEPTION     0
#define DEBUG_BTUSER        0
#define DEBUG_SEQFILE       0
#define DEBUG_IRQ           0
#define PARANOIA_SCHED      0

#define DEBUG_OPEN          0
#define DEBUG_DENTRY        0
#define DEBUG_LOOKUP        0
#define DEBUG_READDIR       0

#define DEBUG_DEVFS         0
#define DEBUG_PROCFS        0
#define DEBUG_RAMFS         0
#define DEBUG_EXT2FS        0

#define DEBUG_SERIAL        0
#define DEBUG_CONSOLE       0
#define DEBUG_CON_SCROLL    0
#define DEBUG_KEYBOARD      0

#define DEBUG_ELF           0
#define DEBUG_MULTIBOOT     0
#define DEBUG_ASSERT        1

#define crash() { int* i = (int*)0; *i = 2137; }

#if DEBUG_ASSERT

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
