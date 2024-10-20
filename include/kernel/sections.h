#pragma once

#include <kernel/string.h>
#include <kernel/compiler.h>

extern char _unpaged_transient_start[], _unpaged_transient_end[];
extern char _text_init_start[], _text_init_end[];
extern char _text_start[], _text_end[];
extern char _rodata_start[], _rodata_end[];
extern char _data_start[], _data_end[];
extern char _bss_start[], _bss_end[];
extern char _end[];

extern char _smodules_data[], _emodules_data[];

#define is_kernel_text(addr) \
    ({ \
        (uintptr_t)(addr) >= (uintptr_t)_text_init_start && (uintptr_t)(addr) <= (uintptr_t)_text_end; \
    })

#define SECTION_READ                (1 << 0)
#define SECTION_WRITE               (1 << 1)
#define SECTION_EXEC                (1 << 2)
#define SECTION_UNPAGED             (1 << 3)
#define SECTION_UNMAP_AFTER_INIT    (1 << 4)

struct section
{
    const char* name;
    void* start;
    void* end;
    int flags;
};

typedef struct section section_t;

extern section_t sections[];

int section_add(const char* name, void* start, void* end, int flags);
void section_free(section_t* section);
void sections_print(void);

#define section_flags_string(flags, buf, size) \
    ({ \
        char* it = buf; \
        it = csnprintf(it, end, flags & SECTION_READ ? "R" : "-"); \
        it = csnprintf(it, end, flags & SECTION_WRITE ? "W" : "-"); \
        it = csnprintf(it, end, flags & SECTION_EXEC ? "X" : "-"); \
        it = csnprintf(it, end, flags & SECTION_UNPAGED ? "U" : "-"); \
        it = csnprintf(it, end, flags & SECTION_UNMAP_AFTER_INIT ? "T" : "-"); \
        buf; \
    })

#define sections_print() \
    { \
        char buf[6]; \
        const char* end = buf + sizeof(buf); \
        section_t* section = sections; \
        for (; section->name; ++section) \
        { \
            log_notice("[sec %p - %p %s] %s", \
                section->start, \
                section->end, \
                section_flags_string(section->flags, buf, end), \
                section->name); \
        } \
    }

#define UNMAP_AFTER_INIT SECTION(.text.init) NOINLINE
