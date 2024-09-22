#pragma once

#include <stdbool.h>
#include <common/macro.h>

#ifndef static_assert
#define static_assert           _Static_assert
#endif

#define UNUSED(x)               (void)(x)

#define __STRINGIFY(x)          #x
#define STRINGIFY(x)            __STRINGIFY(x)
#define EXPAND(x)               x
#define PASTE(a, b)             a##b

#define FALLTHROUGH             __attribute__((__fallthrough__))
#define LIKELY(x)               __builtin_expect(!!(x), 1)
#define UNLIKELY(x)             __builtin_expect(!!(x), 0)
#define ADDR(x)                 ((uintptr_t)(x))
#define PTR(x)                  ((void*)ADDR((x)))
#define CPTR(x)                 ((const void*)(ADDR(x)))

#define SHIFT(ptr, off)         ((typeof(ptr))(ADDR(ptr) + (off)))
#define SHIFT_AS(t, ptr, off)   ((t)(ADDR(ptr) + (off)))

#define ARRAY_SIZE(a)           (sizeof(a) / sizeof(*a))

#define ALIGN_TO(address, size) \
    (((address) + (size) - 1) & (~((size) - 1)))

#define typecheck(type, x) \
    ({ static_assert(_Generic((x), typeof(type): true, default: false), "Invalid type"); })

#define typecheck_fn(type, function) \
    ({ typeof(type) dummy = function; (void)dummy; })

#define typecheck_ptr(x) \
    ({ typeof(x) dummy; (void)sizeof(*dummy); 1; })

#ifndef __clang__

#define DIAG_PUSH() \
    _Pragma("GCC diagnostic push")

#define DIAG_RESTORE() \
    _Pragma("GCC diagnostic pop")

#define _DIAG_IGNORE(x) \
    _Pragma(STRINGIFY(GCC diagnostic ignored x))

#define _DIAG_IGNORE_1(_1) \
    DIAG_PUSH(); \
    _DIAG_IGNORE(_1)

#define _DIAG_IGNORE_2(_1, _2) \
    _DIAG_IGNORE_1(_1); \
    _DIAG_IGNORE(_2)

#define _DIAG_IGNORE_3(_1, _2, _3) \
    _DIAG_IGNORE_2(_1, _2); \
    _DIAG_IGNORE(_3)

#define _DIAG_IGNORE_4(_1, _2, _3, _4) \
    _DIAG_IGNORE_3(_1, _2, _3); \
    _DIAG_IGNORE(_4)

#define DIAG_IGNORE(...) \
    REAL_VAR_MACRO_4( \
        _DIAG_IGNORE_1, \
        _DIAG_IGNORE_2, \
        _DIAG_IGNORE_3, \
        _DIAG_IGNORE_4, \
        __VA_ARGS__)

#else
#define DIAG_PUSH()
#define DIAG_IGNORE(...)
#define DIAG_RESTORE()
#endif


