#pragma once

#ifndef __ASSEMBLER__
#ifdef __GNUC__

#define GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__ * 10)

#define bool    _Bool
#define true    1
#define false   0

#define MAYBE_UNUSED(var)       __attribute__((unused)) var
#define ALIAS(name)             __attribute__((alias(#name)))
#define NOINLINE                __attribute__((noinline))
#define PACKED                  __attribute__((packed))
#define SECTION(x)              __attribute__((section(#x)))
#define COMPILE_ERROR(msg)      __attribute__((__error__(msg)))
#define CLEANUP(fn)             __attribute__((cleanup(fn)))
#define LIKELY                  __attribute__((unused, hot))
#define UNLIKELY                __attribute__((unused, cold))
#define MUST_CHECK(ret)         __attribute__((warn_unused_result)) ret
#define NORETURN(fn)             __attribute__((noreturn)) fn
#define FASTCALL(fn)            __attribute__((regparm(3))) fn

#define __STRINGIFY(x)          #x
#define STRINGIFY(x)            __STRINGIFY(x)
#define EXPAND(x)               x
#define PASTE(a, b)             a##b
#define NOT_USED(x)             (void)(x)

#define static_assert           _Static_assert
#define fallthrough             __attribute__((__fallthrough__))
#define likely(x)               __builtin_expect(!!(x), 1)
#define unlikely(x)             __builtin_expect(!!(x), 0)
#define cptr(a)                 ((const void*)(a))
#define ptr(a)                  ((void*)(a))
#define addr(a)                 ((unsigned int)(a))
#define shift(ptr, off)         ((typeof(ptr))(addr(ptr) + off))
#define shift_as(t, ptr, off)   ((t)(addr(ptr) + off))
#define array_size(a)           (sizeof(a) / sizeof((a)[0]))

#define align(address, size) \
    (((address) + size - 1) & (~(size - 1)))

#define typecheck(type, x) \
    ({ static_assert(_Generic((x), typeof(type): true, default: false), "Invalid type"); })

#define typecheck_fn(type, function) \
    ({ typeof(type) dummy = function; (void)dummy; })

#define typecheck_ptr(x) \
    ({ typeof(x) dummy; (void)sizeof(*dummy); 1; })

#define div(value, div, mod) \
    ({ mod = (value) % (div); (value) / (div); })

#else

#error "Not compatibile compiler!"

#endif // __GNUC__
#endif // __ASSEMBLER__
