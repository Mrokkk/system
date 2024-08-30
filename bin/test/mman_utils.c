#include "mman.h"

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static mapping_t mappings[32];
static unsigned last;

static void mappings_read(location_t* location);

void maps_dump(void)
{
    const char* path = "/proc/self/maps";
    int fd, bytes;
    char buffer[1024];

    fd = open(path, O_RDONLY);

    if (UNLIKELY(fd == -1))
    {
        perror(path);
        return;
    }

    do
    {
        bytes = read(fd, buffer, sizeof(buffer));

        if (UNLIKELY(bytes == -1))
        {
            perror(path);
            goto finish;
        }

        write(STDERR_FILENO, buffer, bytes);
    }
    while (bytes == sizeof(buffer));

finish:
    close(fd);
}

[[noreturn]] void die(location_t* location, const char* fmt, ...)
{
    va_list args;
    char buffer[1024];
    char* cur = buffer;

    va_start(args, fmt);
    cur += vsnprintf(cur, sizeof(buffer), fmt, args);
    va_end(args);

    FAIL_L(location->file, location->line, buffer);

    maps_dump();

    exit(EXIT_FAILURE);
}

[[noreturn]] void perror_die_impl(location_t location, const char* fmt, ...)
{
    va_list args;
    char buffer[1024];
    char* cur = buffer;

    va_start(args, fmt);
    cur += vsnprintf(cur, sizeof(buffer), fmt, args);
    va_end(args);

    cur += snprintf(cur, sizeof(buffer) - (cur - buffer), ": %s", strerror(errno));

    FAIL_L(location.file, location.line, buffer);

    exit(EXIT_FAILURE);
}

void* mmap_wrapped(void* addr, size_t len, int prot, int flags, int fd, size_t off, location_t location)
{
    void* ret = mmap(addr, len, prot, flags, fd, off);
    mappings_read(&location);
    return ret;
}

int mprotect_wrapped(void* addr, size_t len, int prot, location_t location)
{
    int ret = mprotect(addr, len, prot);
    mappings_read(&location);
    return ret;
}

#define BITFLAG(f) \
    if (value & (f)) \
    { \
        if (continuation) \
        { \
            cur += strlcpy(cur, "|", size - (cur - buffer)); \
        } \
        cur += strlcpy(cur, #f, size - (cur - buffer)); \
        value ^= f; \
        continuation = 1; \
    }

static char* prot_flags_get(int value, char* buffer, size_t size)
{
    char* cur = buffer;
    int continuation = 0;
    BITFLAG(PROT_READ);
    BITFLAG(PROT_WRITE);
    BITFLAG(PROT_EXEC);
    if (!continuation)
    {
        strlcpy(cur, "PROT_NONE", size);
    }
    return buffer;
}

static void mapping_add(uintptr_t start, uintptr_t end, const char* prot)
{
    int flags = 0;
    mapping_t* mapping = &mappings[last++];

    mapping->start = start;
    mapping->size = end - start;
    mapping->ptr = PTR(start);

    flags |= *prot++ == 'r' ? PROT_READ : 0;
    flags |= *prot++ == 'w' ? PROT_WRITE : 0;
    flags |= *prot++ == 'x' ? PROT_EXEC : 0;

    mapping->flags = flags;
}

static void mapping_read(char* it, location_t* location)
{
    char* endptr;
    char* start_str = strtok(it, " -");
    char* end_str = strtok(NULL, " ");
    char* prot = strtok(NULL, " ");
    uintptr_t start, end;

    if (UNLIKELY(!start_str)) die(location, "cannot read start of mapping");
    if (UNLIKELY(!end_str)) die(location, "cannot read end of mapping");
    if (UNLIKELY(!prot)) die(location, "cannot read prot of mapping");

    start = strtoul(start_str, &endptr, 16);
    end = strtoul(end_str, &endptr, 16);

    mapping_add(start, end, prot);
}

static void mappings_read(location_t* location)
{
    const char* path = "/proc/self/maps";
    int fd, bytes;
    char buffer[0x1000];
    char* newline = NULL;
    char* it;

    last = 0;

    fd = MUST_SUCCEED(open(path, O_RDONLY));

    do
    {
        it = buffer;
        bytes = MUST_SUCCEED(read(fd, buffer, sizeof(buffer) - 1));
        buffer[bytes] = 0;

        do
        {
            newline = strchr(it, '\n');

            if (newline && *(newline + 1) == '\0')
            {
                break;
            }

            mapping_read(it, location);

            it = newline + 1;
        }
        while (newline);
    }
    while (bytes == sizeof(buffer));

    mappings[last].start = 0;

    close(fd);
}

mapping_t* expect_mapping_impl(uintptr_t start, size_t size, int flags, location_t location)
{
    char expected_prot[64];
    char actual_prot[64];
    mapping_t* it = mappings;

    for (; it->start; ++it)
    {
        if (it->start != start)
        {
            continue;
        }

        if (it->size != size)
        {
            FAIL_L(location.file, location.line, "incorrect size: expected %#lx, got %#lx", size, it->size);
            return NULL;
        }

        if (it->flags != flags)
        {
            FAIL_L(location.file, location.line, "expected %s, got %s",
                prot_flags_get(flags, expected_prot, sizeof(expected_prot)),
                prot_flags_get(it->flags, actual_prot, sizeof(actual_prot)));
            return NULL;
        }

        return it;
    }

    FAIL_L(location.file, location.line, "no mapping {start: %#x, size: %#lx, prot: %#s}",
        start,
        size,
        prot_flags_get(flags, expected_prot, sizeof(expected_prot)));

    return NULL;
}

void verify_access_impl(mapping_t* m, location_t location)
{
    if (UNLIKELY(!m))
    {
        return;
    }
    if (m->flags & PROT_READ)
    {
        EXPECT_EXIT_WITH_L(0, location.file, location.line)
        {
            uintptr_t* data = m->ptr;
            EXPECT_EQ(*data, 0);
            exit(FAILED_EXPECTATIONS());
        }
    }
    else
    {
        EXPECT_KILLED_BY_L(SIGSEGV, location.file, location.line)
        {
            uintptr_t* data = m->ptr;
            EXPECT_EQ(*data, 0);
            exit(FAILED_EXPECTATIONS());
        }
    }
    if (m->flags & PROT_WRITE)
    {
        EXPECT_EXIT_WITH_L(0, location.file, location.line)
        {
            uintptr_t* data = m->ptr;
            *data = 32;
            *data = 0;
            exit(FAILED_EXPECTATIONS());
        }
    }
    else
    {
        EXPECT_KILLED_BY_L(SIGSEGV, location.file, location.line)
        {
            uintptr_t* data = m->ptr;
            *data = 32;
            *data = 0;
            exit(FAILED_EXPECTATIONS());
        }
    }
}
