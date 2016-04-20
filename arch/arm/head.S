#define ENTRY(x) \
    .global x; \
    .weak x; \
    x:

.section ".text.boot"

ENTRY(interrupt_vector_table)
    .long _start
    .long _start
    .long swi_handler
    b .
    b .
    b .
    b .
    b .

ENTRY(_start)

    ldr sp, =stack_top

    ldr r4, =_sbss
    ldr r9, =_ebss
    mov r5, #0
    mov r6, #0
    mov r7, #0
    mov r8, #0
    b 2f

1:
    stmia r4!, {r5-r8}

2:
    cmp r4, r9
    blo 1b

    bl arch_setup
    bl kmain
1:
    b 1b


#define __syscall0(call) \
    ENTRY(call) \
        mov r0, #0; \
        bx lr; \
        nop;

#define __syscall1(call, ...) \
    ENTRY(call) \
        mov r0, #0; \
        bx lr; \
        nop;

#define __syscall2(call, ...) \
    ENTRY(call) \
        mov r0, #0; \
        bx lr; \
        nop;

#define __syscall3(call, ...) \
    ENTRY(call) \
        mov r0, #0; \
        bx lr; \
        nop;

#define __syscall4(call, ...) \
    ENTRY(call) \
        mov r0, #0; \
        bx lr; \
        nop;

ENTRY(swi_handler)
    bl swi
    bx lr

#include <kernel/unistd.h>

.section .stack
ENTRY(kstack)
    .skip 0x10000 /* 4 kB */
ENTRY(stack_top)

