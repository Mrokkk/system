#pragma once

#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <common/compiler.h>

#include "test.h"

typedef struct mapping mapping_t;
typedef struct location location_t;

struct mapping
{
    uintptr_t   start;
    size_t      size;
    int         flags;
    off_t       offset;
    const char* path;
    void*       ptr;
    mapping_t*  next;
};

struct location
{
    const char* file;
    size_t      line;
};

#define LOCATION() \
    (location_t){ \
        .file = __FILE__, \
        .line = __LINE__, \
    }

void* mmap_wrapped(void* addr, size_t len, int prot, int flags, int fd, size_t off, location_t location);
int mprotect_wrapped(void* addr, size_t len, int prot, location_t location);

mapping_t* mappings_read(location_t location);
mapping_t* expect_mapping_impl(uintptr_t start, size_t size, int flags, off_t offset, const char* path, location_t location);
void verify_access_impl(mapping_t* m, location_t location);

void maps_dump(void);

[[noreturn]] void die(location_t* location, const char* fmt, ...);
[[noreturn]] void perror_die_impl(location_t location, const char* fmt, ...);

#define MMAP(...) \
    mmap_wrapped(__VA_ARGS__, LOCATION())

#define MPROTECT(...) \
    mprotect_wrapped(__VA_ARGS__, LOCATION())

#define PERROR_DIE(fmt, ...) \
    perror_die_impl(LOCATION(), fmt, ##__VA_ARGS__)

#define MUST_SUCCEED(call) \
    ({ \
        typeof(call) ret = call; \
        if (UNLIKELY(ret == (typeof(ret))-1)) \
        { \
            PERROR_DIE(#call); \
        } \
        ret; \
    })

#define MAPPINGS_FOR_EACH(name) \
    for (mapping_t* name = mappings_read(LOCATION()); name; name = name->next)

#define EXPECT_MAPPING(start, size, flags, offset, path) \
    expect_mapping_impl(start, size, flags, offset, path, LOCATION())

#define EXPECT_MAPPING_CHECK_ACCESS(start, size, flags, offset, path) \
    ({ \
        mapping_t* m = EXPECT_MAPPING(start, size, flags, offset, path); \
        if (LIKELY(m)) \
        { \
            verify_access_impl(m, LOCATION()); \
        } \
    })
