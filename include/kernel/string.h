#pragma once

#include <arch/string.h>
#include <kernel/types.h>

#ifndef __HAVE_ARCH_STRLEN
size_t strlen(const char* s);
#endif

#ifndef __HAVE_ARCH_STRNLEN
size_t strnlen(const char* s, size_t count);
#endif

#ifndef __HAVE_ARCH_STRCPY
char* strcpy(char* __restrict, const char* __restrict);
#endif

#ifndef __HAVE_ARCH_STRNCPY
char* strncpy(char* __restrict, const char* __restrict, size_t count);
#endif

int strcmp(const char*, const char*);

int strncmp(const char*, const char*, size_t);
char* strchr(const char*, int);
char* strrchr(const char*, int);
void bcopy(const void*, void*, size_t);
void* memcpy(void* dest, const void* src, size_t n);
void* memcpyw(unsigned short*, const unsigned short*, size_t);
void* memset(void*, int, size_t);
void* memsetw(unsigned short*, unsigned short, size_t);

#define copy_struct(dest, src) \
    memcpy(dest, src, sizeof(typeof(*src)))

#define copy_array(dest, src, count) \
    memcpy(dest, src, sizeof(typeof(*src)) * (count))

static inline char* word_read(char* string, char* output)
{
    while (*string != '\n' && *string != '\0' && *string != ' ')
    {
        *output++ = *string++;
    }

    string++;
    *output = 0;

    return string;
}
