#pragma once

#include <stdint.h>
#include <kernel/compiler.h>

#include <arch/segment.h>
#include <arch/register.h>
#include <kernel/trace.h>
#include <kernel/page.h>
#include <kernel/printk.h>
#include <kernel/process.h>
#include <kernel/sections.h>

void stack_dump(const uint32_t* stack, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        log_exception("%08x: %08x", stack + i, stack[i]);
    }
}

static inline uint32_t remaining_stack_prev(struct process* p)
{
    return PAGE_SIZE - (addr(p->mm->kernel_stack) - esp_get());
}

static inline uint32_t remaining_stack_next(struct process* p)
{
    return PAGE_SIZE - (addr(p->mm->kernel_stack) - p->context.esp);
}

static inline void paranoia(struct process*, struct process* next)
{
    extern pid_t last_pid;
    extern void ret_from_syscall();

    uint32_t* stack;
    uint32_t stack_end;
    uint32_t process_switch_addr;

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

    if (unlikely(addr(stack) < stack_end))
    {
        cli();
        stack_dump(stack, 32);
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
        stack_dump(stack, 32);
        panic("process %u:%x: memory corruption; %x = %x; expected: %x",
            next->pid,
            next,
            stack_end,
            *(uint32_t*)stack_end,
            STACK_MAGIC);
    }

    asm volatile("movl $1f, %0" : "=m" (process_switch_addr));

    if (next->context.eip == process_switch_addr)
    {
        uint32_t gs = *stack & 0xffff;
        if (gs != KERNEL_DS && gs != USER_DS)
        {
            cli();
            stack_dump(stack, 32);
            panic("process %u:%x gs = %x, expected = %x or %x;",
                next->pid,
                next,
                *stack,
                KERNEL_DS,
                USER_DS);
        }
    }
    else if (next->context.eip == addr(&ret_from_syscall))
    {
        struct pt_regs* regs = ptr(stack);
        switch (next->type)
        {
            #define CHECK(seg, val) if (seg != val) { cli(); stack_dump(stack, 32); panic(#seg " = %x", seg); }
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
        "movl $1f, %1;"
        "pushl %3;"
        "jmp __process_switch;"
        "1:"
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
