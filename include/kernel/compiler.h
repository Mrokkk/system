#pragma once

#ifdef __GNUC__

#define GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__ * 10)

#define asm __asm__
#define __gnu_inline static __inline__
#define inline __inline__ __attribute__((always_inline))

#define __used                  __attribute__ ((used))
#define __maybe_unused          __attribute__ ((unused))
#define __visible               __attribute__ ((externally_visible))
#define __alias(name)           __attribute__ ((alias(#name)))
#define __noinline              __attribute__ ((noinline))
#define __noclone               __attribute__ ((noclone))
#define __weak                  __attribute__ ((weak))
#define __deprecated            __attribute__ ((deprecated))
#define __noreturn              __attribute__ ((noreturn))
#define __packed                __attribute__ ((packed))
#define __naked                 __attribute__ ((naked)) __noinline
#define __optimize(x)           __attribute__ ((optimize(#x)))
#define __section(x)            __attribute__ ((section(#x)))
#define __aligned(x)            __attribute__ ((used, aligned(x)))
#define __compile_error(msg)    __attribute__((__error__(msg)))
#define fallthrough             __attribute__((__fallthrough__))

#define __strong_alias(name)    __alias(name)
#define __weak_alias(name)      __weak __alias(name)

#define expand(x)               x
#define paste(a, b)             a##b
#define not_used(x)             (void)(x)
#define likely(x)               __builtin_expect(!!(x), 1)
#define unlikely(x)             __builtin_expect(!!(x), 0)
#define unreachable()           do { } while (1)

#define align_to_block_size(address, size) \
    (((address) + size - 1) & (~(size - 1)))

#define __stringify(x)  #x
#define stringify(x)    __stringify(x)
#define cptr(a) ((const void*)(a))
#define ptr(a) ((void*)(a))
#define addr(a) ((unsigned int)(a))

#define typecheck(type, x) \
    ({ type __dummy; typeof(x) __dummy2; (void)(&__dummy == &__dummy2); 1; })

#define typecheck_fn(type, function) \
    ({ typeof(type) __tmp = function; (void)__tmp; })

#define typecheck_ptr(x) \
    ({ typeof(x) __dummy; (void)sizeof(*__dummy); 1; })

#else

#error "Not compatibile compiler!"

#endif // __GNUC__
