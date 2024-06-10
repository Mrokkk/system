#pragma once

#include <stdint.h>
#include <kernel/ksyms.h>
#include <kernel/usyms.h>

#define BACKTRACE_MAX_RECURSION 32
#define BACKTRACE_SYMNAME_LEN   128

struct process;

typedef struct stack_frame
{
    struct stack_frame* next;
    void* ret;
} stack_frame_t;

void* backtrace_start(void);
void* backtrace_next(void** data);

void* backtrace_user_start(struct process* p, uint32_t eip, uint32_t esp, uint32_t ebp);
void* backtrace_user_next(void** data);

size_t do_backtrace_process(struct process* p, void** buffer, size_t count);
void backtrace_exception(struct pt_regs* regs);

#define memory_dump(log_fn, addr, count) \
    do \
    { \
        uint32_t* s = ptr(addr); \
        for (uint32_t i = 0; i < count; ++i) \
        { \
            log_fn("%08x: %08x", s + i, s[i]); \
        } \
    } while (0)

#define backtrace_dump(log_fn) \
    do \
    { \
        void* data = backtrace_start(); \
        void* ret; \
        char buffer[BACKTRACE_SYMNAME_LEN]; \
        unsigned depth = 0; \
        log_fn("backtrace:"); \
        while ((ret = backtrace_next(&data)) && depth < BACKTRACE_MAX_RECURSION) \
        { \
            ksym_string(buffer, addr(ret)); \
            log_fn("%s", buffer); \
            ++depth; \
        } \
    } while (0)

#define backtrace_process(p, log_fn, ...) \
    do \
    { \
        char buffer[BACKTRACE_SYMNAME_LEN]; \
        void* bt[BACKTRACE_MAX_RECURSION]; \
        for (size_t i = 0; i < do_backtrace_process(p, bt, BACKTRACE_MAX_RECURSION); ++i) \
        { \
            ksym_string(buffer, addr(bt[i])); \
            log_fn(__VA_ARGS__, "%s\n", buffer); \
        } \
    } while (0)

#define backtrace_user(log_fn, regs, syms, ...) \
    do \
    { \
        char buffer[BACKTRACE_SYMNAME_LEN]; \
        void* data = backtrace_user_start(process_current, (regs)->eip, (regs)->esp, (regs)->ebp); \
        void* ret; \
        unsigned depth = 0; \
        log_fn(__VA_ARGS__ "backtrace: "); \
        while ((ret = backtrace_user_next(&data)) && depth < BACKTRACE_MAX_RECURSION) \
        { \
            usym_string(buffer, addr(ret), syms); \
            log_fn(__VA_ARGS__ "%s", buffer); \
            ++depth; \
        } \
    } while (0)
