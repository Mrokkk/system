#ifndef __X86_SYSTEM_H_
#define __X86_SYSTEM_H_

#include <kernel/compiler.h>
#include <arch/segment.h>

#define mb() \
    asm volatile("" : : : "memory")

#define flags_save(x) \
    asm volatile("pushfl; popl %0" : "=r" (x) : /*  */ : "memory")

#define flags_restore(x) \
    asm volatile("pushl %0; popfl" : /*  */ : "r" (x) : "memory")

#define sti() \
    asm volatile("sti" : : : "memory")

#define cli() \
    asm volatile("cli" : : : "memory")

/* As long I don't support SMP I don't
 * need a spinlock on irq_save and irq_restore
 */
#define irq_save(x) \
    { flags_save(x); cli(); }

#define irq_restore(x) \
    { flags_restore(x); }

#define nop() \
    asm volatile("nop")

#define halt() \
    asm volatile("hlt")

#include <kernel/process.h>

/* Context switching */
#define process_switch(prev, next) \
    do {                                \
        asm volatile(                   \
            "push %%gs;"                \
            "pushl %%ebx;"              \
            "pushl %%ecx;"              \
            "pushl %%esi;"              \
            "pushl %%edi;"              \
            "pushl %%ebp;"              \
            "movl %%esp, %0;"           \
            "movl %2, %%esp;"           \
            "movl $1f, %1;"             \
            "pushl %3;"                 \
            "jmp __process_switch;"     \
            "1: "                       \
            "popl %%ebp;"               \
            "popl %%edi;"               \
            "popl %%esi;"               \
            "popl %%ecx;"               \
            "popl %%ebx;"               \
            "pop %%gs;"                 \
            : "=m" (prev->context.esp), \
              "=m" (prev->context.eip)  \
            : "m" (next->context.esp),  \
              "m" (next->context.eip),  \
              "a" (prev), "d" (next));  \
    } while (0)

extern unsigned long loops_per_sec;
extern void do_delay();
extern void irq_enable(unsigned int);
extern void delay(unsigned int);
extern void reboot();
extern void shutdown();
unsigned int ram_get();

static inline void udelay(unsigned long usecs) {

    usecs *= loops_per_sec/1000000;

    do_delay(usecs);

}

#endif /* __X86_SYSTEM_H_ */
