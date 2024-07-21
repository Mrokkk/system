#pragma once

#include <stddef.h>
#include <stdint.h>
#include <kernel/limits.h>
#include <arch/processor.h>

#define BACKTRACE_MAX_RECURSION 32
#define BACKTRACE_SYMNAME_LEN   128

struct process;

typedef struct stack_frame stack_frame_t;

struct stack_frame
{
    stack_frame_t* next;
    void*          ret;
};

void backtrace_process(const struct process* p, int (*print_func)(), void* arg0);
void backtrace_exception(const pt_regs_t* regs);
void backtrace_dump(const char* severity);
void backtrace_user(const char* severity, const pt_regs_t* regs, const char* prefix);

void memory_dump_impl(const char* severity, const uint32_t* addr, size_t count);

#define memory_dump(severity, addr, count) \
    memory_dump_impl(severity, (uint32_t*)(addr), count)
