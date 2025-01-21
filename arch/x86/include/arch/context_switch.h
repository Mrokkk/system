#pragma once

#include <stdint.h>
#include <kernel/compiler.h>

#include <arch/segment.h>
#include <arch/register.h>

#include <kernel/debug.h>
#include <kernel/printk.h>
#include <kernel/process.h>
#include <kernel/sections.h>
#include <kernel/backtrace.h>
#include <kernel/page_table.h>

#ifdef __i386__
static inline void paranoia(process_t*, process_t* next)
{
    uintptr_t* stack;
    uintptr_t stack_end;

    switch (next->type)
    {
        case USER_PROCESS:
        case KERNEL_PROCESS:
            break;
        default:
            panic("process %u:%#x: invalid type %#x",
                next->pid,
                next,
                next->type);
    }

    if (unlikely((unsigned long)next->pid > (unsigned long)last_pid))
    {
        panic("process %u:%#x: memory corruption; pid[%#x] = %u",
            next->pid,
            next,
            &next->pid,
            next->pid);
    }

    stack = ptr(next->context.esp);
    stack_end = addr(next->kernel_stack) - PAGE_SIZE;

    if (unlikely(!stack))
    {
        panic("stack is null");
        return;
    }

    if (unlikely(addr(stack) < stack_end))
    {
        cli();
        memory_dump(KERN_CRIT, stack, 32);
        panic("process %u:%#x: kernel stack overflow at %#x; esp = %#x, stack_end = %#x",
            next->pid,
            next,
            next->context.eip,
            next->context.esp,
            stack_end);
    }

    if (unlikely(*(uintptr_t*)stack_end != STACK_MAGIC))
    {
        cli();
        memory_dump(KERN_CRIT, stack, 32);
        panic("process %u:%#x: memory corruption; %#x = %#x; expected: %#x",
            next->pid,
            next,
            stack_end,
            *(uintptr_t*)stack_end,
            STACK_MAGIC);
    }

    if (next->context.eip == addr(&context_restore))
    {
    }
    else if (next->context.eip == addr(&exit_kernel))
    {
        pt_regs_t* regs = ptr(stack);
        switch (next->type)
        {
            #define CHECK(seg, val) \
                if (seg != val) \
                { \
                    cli(); \
                    memory_dump(KERN_CRIT, stack, 32); panic(#seg " = %#x", seg); \
                }

            case USER_PROCESS:
                CHECK(regs->cs, USER_CS);
                CHECK(regs->ds, USER_DS);
                CHECK(regs->es, USER_DS);
                CHECK(regs->fs, USER_DS);
                CHECK(regs->gs, USER_TLS);
                CHECK(regs->ss, USER_DS);
                break;
            case KERNEL_PROCESS:
                CHECK(regs->cs, KERNEL_CS);
                CHECK(regs->ds, KERNEL_DS);
                CHECK(regs->es, KERNEL_DS);
                CHECK(regs->fs, KERNEL_DS);
                CHECK(regs->gs, KERNEL_DS);
                if (!is_kernel_text(regs->eip))
                {
                    panic("regs->eip = %#x", regs->eip);
                }
                break;
        }
    }
    else
    {
        panic("process %d, wrong eip = %p @ %p", next->pid, next->context.eip, &next->context.eip);
    }
}

static inline void process_switch(process_t* prev, process_t* next)
{
    cli();
#if PARANOIA_SCHED
    paranoia(prev, next);
#endif
    asm volatile(
        "pushl %%ebx;"
        "pushl %%ecx;"
        "pushl %%esi;"
        "pushl %%edi;"
        "pushl %%ebp;"
        "push $0xdead;"
        "movl %%esp, %0;"
        "movl %2, %%esp;"
        "movl $context_restore, %1;"
        "pushl %3;"
        "jmp __process_switch;"
        ".global context_restore; context_restore:"
        "popl %%ecx;"
#if PARANOIA_SCHED
        "cmp $0xdead, %%ecx;"
        "jne __process_switch_bug;"
#endif
        "popl %%ebp;"
        "popl %%edi;"
        "popl %%esi;"
        "popl %%ecx;"
        "popl %%ebx;"
        "sti;"
        : "=m" (prev->context.esp),
          "=m" (prev->context.eip)
        : "m" (next->context.esp),
          "m" (next->context.eip),
          "a" (prev), "d" (next)
        : "memory");
}
#elif defined(__x86_64__)
static inline void process_switch(process_t*, process_t*)
{
}
#endif
