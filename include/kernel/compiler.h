#ifndef __COMPILER_H_
#define __COMPILER_H_

#ifdef __GNUC__

#if defined(__STDC__)
#    define PREDEF_STANDARD_C_1989
#    if defined(__STDC_VERSION__)
#        if (__STDC_VERSION__ >= 199409L)
#            define PREDEF_STANDARD_C_1994
#        endif
#        if (__STDC_VERSION__ >= 199901L)
#            define PREDEF_STANDARD_C_1999
#        endif
#    endif
#endif

#define GCC_VERSION (__GNUC__ * 100         \
                     + __GNUC_MINOR__ * 10)

#if !defined(PREDEF_STANDARD_C_1989)
#    define const
#    define volatile
#endif

#if __STRICT_ANSI__ || !defined(__STDC_VERSION__)
#    define asm __asm__
#    define __gnu_inline static __inline__
#    define inline __inline__ __attribute__((always_inline))
#else
#    define __gnu_inline static inline
#endif

#define __used                  __attribute__ ((used))
#define __unused                __attribute__ ((unused))
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

#define __strong_alias(name)    __alias(name)
#define __weak_alias(name)      __weak __alias(name)

#define expand(x)               x
#define paste(a, b)             a##b
#define not_used_var(x)         (void)x
#define not_used_param(x)       (void)x
#define likely(x)               __builtin_expect((x), 1)
#define unlikely(x)             __builtin_expect((x), 0)
#define unreachable()           do { } while (1)

#define __stringify(x)  #x
#define stringify(x)    __stringify(x)

#else

#warning "Not compatibile compiler!"

#define __FILENAME__ __FILE__
#define __optimize(x)
#define __stringify(x)  #x
#define stringify(x)    __stringify(x)
#define __aligned(x)
#define __noreturn
#ifndef __VERSION__
#define __VERSION__ "0.0"
#endif

#endif

#endif
