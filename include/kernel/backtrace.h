#pragma once

#include <stdint.h>
#include <kernel/ksyms.h>
#include <kernel/limits.h>

#define BACKTRACE_MAX_RECURSION 32
#define BACKTRACE_SYMNAME_LEN   128

struct process;

typedef struct stack_frame
{
    struct stack_frame* next;
    void* ret;
} stack_frame_t;

struct user_address
{
    uint32_t vaddr;
    uint32_t file_offset;
    char path[PATH_MAX];
};

typedef struct user_address user_address_t;

void* backtrace_start(void);
void* backtrace_next(void** data);

void* backtrace_user_start(struct process* p, uint32_t eip, uint32_t esp, uint32_t ebp);
void* backtrace_user_next(void** data, user_address_t* addr);
void backtrace_user_format(user_address_t* addr, char* buffer);

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

// FIXME: there's a leak of struct bt_data when BACKTRACE_MAX_RECURSION is hit
#define backtrace_user(log_fn, regs, ...) \
    do \
    { \
        char buffer[BACKTRACE_SYMNAME_LEN]; \
        log_fn(__VA_ARGS__ "backtrace: "); \
        user_address_t __addr; \
        void* data = backtrace_user_start(process_current, (regs)->eip, (regs)->esp, (regs)->ebp); \
        if (data) \
        { \
            unsigned depth = 0; \
            while ((backtrace_user_next(&data, &__addr)) && depth < BACKTRACE_MAX_RECURSION) \
            { \
                backtrace_user_format(&__addr, buffer); \
                log_fn(__VA_ARGS__ "%s", buffer); \
                ++depth; \
            } \
        } \
    } while (0)
