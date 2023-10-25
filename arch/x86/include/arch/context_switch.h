#pragma once

#include <stdint.h>
#include <kernel/compiler.h>

#include <arch/segment.h>
#include <arch/register.h>

#include <kernel/page.h>
#include <kernel/debug.h>
#include <kernel/printk.h>
#include <kernel/process.h>
#include <kernel/sections.h>
#include <kernel/backtrace.h>

static inline void paranoia(struct process*, struct process* next)
{
    uint32_t* stack;
    uint32_t stack_end;

    switch (next->type)
    {
        case USER_PROCESS:
        case KERNEL_PROCESS:
            break;
        default:
            panic("process %u:%x: invalid type %x",
                next->pid,
                next,
                next->type);
    }

    if (unlikely((unsigned long)next->pid > (unsigned long)last_pid))
    {
        panic("process %u:%x: memory corruption; pid[%x] = %u",
            next->pid,
            next,
            &next->pid,
            next->pid);
    }

    stack = ptr(next->context.esp);
    stack_end = addr(next->mm->kernel_stack) - PAGE_SIZE;

    if (unlikely(!stack))
    {
        panic("stack is null");
        return;
    }

    if (unlikely(addr(stack) < stack_end))
    {
        cli();
        memory_dump(log_critical, stack, 32);
        panic("process %u:%x: kernel stack overflow at %x; esp = %x, stack_end = %x",
            next->pid,
            next,
            next->context.eip,
            next->context.esp,
            stack_end);
    }

    if (unlikely(*(uint32_t*)stack_end != STACK_MAGIC))
    {
        cli();
        memory_dump(log_critical, stack, 32);
        panic("process %u:%x: memory corruption; %x = %x; expected: %x",
            next->pid,
            next,
            stack_end,
            *(uint32_t*)stack_end,
            STACK_MAGIC);
    }

    if (next->context.eip == addr(&context_restore))
    {
        struct context_switch_frame* regs = ptr(stack);
        if (regs->gs != KERNEL_DS && regs->gs != USER_DS)
        {
            cli();
            memory_dump(log_critical, stack, 32);
            panic("process %u:%x gs = %x, expected = %x or %x;",
                next->pid,
                next,
                regs->gs,
                KERNEL_DS,
                USER_DS);
        }
    }
    else if (next->context.eip == addr(&exit_kernel))
    {
        struct pt_regs* regs = ptr(stack);
        switch (next->type)
        {
            #define CHECK(seg, val) \
                if (seg != val) \
                { \
                    cli(); \
                    memory_dump(log_critical, stack, 32); panic(#seg " = %x", seg); \
                }

            case USER_PROCESS:
                CHECK(regs->cs, USER_CS);
                CHECK(regs->ds, USER_DS);
                CHECK(regs->es, USER_DS);
                CHECK(regs->fs, USER_DS);
                CHECK(regs->gs, USER_DS);
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
                    panic("regs->eip = %x", regs->eip);
                }
                break;
        }
    }
    else
    {
        panic("process %d, wrong eip = %x", next->pid, next->context.eip);
    }
}

static inline void process_switch(struct process* prev, struct process* next)
{
#if PARANOIA_SCHED
    paranoia(prev, next);
#endif
    asm volatile(
        "pushl %%ebx;"
        "pushl %%ecx;"
        "pushl %%esi;"
        "pushl %%edi;"
        "pushl %%ebp;"
        "pushl %%gs;"
        "movl %%esp, %0;"
        "movl %2, %%esp;"
        "movl $context_restore, %1;"
        "pushl %3;"
        "jmp __process_switch;"
        ".global context_restore; context_restore:"
        "popl %%gs;"
        "popl %%ebp;"
        "popl %%edi;"
        "popl %%esi;"
        "popl %%ecx;"
        "popl %%ebx;"
        : "=m" (prev->context.esp),
          "=m" (prev->context.eip)
        : "m" (next->context.esp),
          "m" (next->context.eip),
          "a" (prev), "d" (next));
}
