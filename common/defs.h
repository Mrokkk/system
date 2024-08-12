#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef UNLIKELY
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

#define CONCAT(a, b) a##b

#ifndef NAME_PREFIX
#define __NAME(name, ...) name
#else
#define __NAME(name, prefix) CONCAT(prefix, name)
#endif

#define NAME(name) __NAME(name, NAME_PREFIX)

#define PRE(...)

#ifdef MAKE_ALIAS
#define POST(name) \
    extern __typeof(NAME(name)) name \
        __attribute__((weak, alias(STRINGIFY(NAME(name)))))
#else
#define POST(...)
#endif

#ifdef FUNCNAME

#define HEADER(x)               <x.c>
#define FUNC_INCLUDE(a, arch)   HEADER(common/ arch/ a)

#if __has_include(FUNC_INCLUDE(FUNCNAME, ARCH))
#define IS_ARCH_DEFINED 1
#else
#define IS_ARCH_DEFINED 0
#endif

#else
#define IS_ARCH_DEFINED 0
#endif

void* NAME(memset)(void* s, int c, size_t n);
void* NAME(memcpy)(void* dest, const void* src, size_t n);
int NAME(memcmp)(const void* s1, const void* s2, size_t n);

size_t NAME(strlen)(const char* s);
size_t NAME(strnlen)(const char* s, size_t count);

size_t NAME(strspn)(const char* s, const char* accept);
size_t NAME(strcspn)(const char* s, const char* reject);
