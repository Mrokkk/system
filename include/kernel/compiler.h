#pragma once

#ifndef __ASSEMBLER__
#ifdef __GNUC__

#include <stdint.h>
#include <stdbool.h>
#include <common/macro.h>
#include <common/compiler.h>
#include <arch/bitsperlong.h>

#define MAYBE_UNUSED(var)       __attribute__((unused)) var
#define ALIAS(name)             __attribute__((alias(#name)))
#define NOINLINE                __attribute__((noinline))
#define PACKED                  __attribute__((packed))
#define SECTION(x)              __attribute__((section(#x)))
#define COMPILE_ERROR(msg)      __attribute__((__error__(msg)))
#define CLEANUP(fn)             __attribute__((cleanup(fn)))
#define MUST_CHECK(ret)         __attribute__((warn_unused_result)) ret
#define NONNULL(...)            __attribute__((nonnull(__VA_ARGS__)))
#define RETURNS_NONNULL(fn)     __attribute__((returns_nonnull)) fn
#define NORETURN(fn)            __attribute__((noreturn)) fn
#define FASTCALL(fn)            __attribute__((regparm(3))) fn
#define ALIGN(x)                __attribute__((aligned(x)))
#define WEAK                    __attribute__((weak))
#define FORMAT(...)             __attribute__((format(__VA_ARGS__)))
#define ACCESS_ONCE(x)          (*(volatile typeof(x)*)&(x))

#define fallthrough             FALLTHROUGH
#define likely(x)               LIKELY(x)
#define unlikely(x)             UNLIKELY(x)
#define cptr(a)                 CPTR(a)
#define ptr(a)                  PTR(a)
#define addr(a)                 ADDR(a)
#define shift(ptr, off)         SHIFT(ptr, off)
#define shift_as(t, ptr, off)   SHIFT_AS(t, ptr, off)
#define array_size(a)           ARRAY_SIZE(a)
#define align(address, size)    ALIGN_TO(address, size)

#else

#error "Not compatibile compiler!"

#endif // __GNUC__
#endif // __ASSEMBLER__
