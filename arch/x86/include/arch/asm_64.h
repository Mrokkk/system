#pragma once

#ifdef __ASSEMBLER__

#define SAVE_ALL(has_error_code) \
    .if has_error_code == 0; \
        sub $8, %rsp; \
    .endif; \
    push %rax; \
    mov %gs, %rax; \
    push %rax; \
    mov %fs, %rax; \
    push %rax; \
    mov %es, %rax; \
    push %rax; \
    mov %ds, %rax; \
    push %rax; \
    mov $KERNEL_DS, %rax;         \
    mov %ax, %ds;                 \
    mov %ax, %es;                 \
    mov %ax, %fs;                 \
    mov %ax, %ss;                 \
    mov 32(%rsp), %rax;           \
    push %r15;                    \
    push %r14;                    \
    push %r13;                    \
    push %r12;                    \
    push %r11;                    \
    push %r10;                    \
    push %r9;                     \
    push %r8;                     \
    push %rbp;          /* ebp */ \
    push %rdi;          /* edi */ \
    push %rsi;          /* esi */ \
    push %rdx;          /* edx */ \
    push %rcx;          /* ecx */ \
    push %rbx;          /* ebx */ \
    SAVE_USER_ESP;

#define SAVE_USER_ESP \

    /*
    cmpl $KERNEL_CS, REGS_CS(%rsp); \
    je 1f; \
    movabs $process_current, %rcx; \
    mov (%rcx), %rcx; \
    mov REGS_RSP(%rsp), %rdx; \
    mov %rdx, CONTEXT_RSP2(%rcx); \
    1:
    */

#define RESTORE_ALL \
    pop %rbx; \
    pop %rcx; \
    pop %rdx; \
    pop %rsi; \
    pop %rdi; \
    pop %rbp; \
    pop %r8; \
    pop %r9; \
    pop %r10; \
    pop %r11; \
    pop %r12; \
    pop %r13; \
    pop %r14; \
    pop %r15; \
    pop %rax; \
    mov %ax, %ds; \
    pop %rax; \
    mov %ax, %es; \
    pop %rax; \
    mov %ax, %fs; \
    pop %rax; \
    mov %ax, %gs; \
    pop %rax; \
    add $8, %rsp;

#define __pic_isr(nr) \
    ENTRY(isr_##nr) \
        cli; \
        SAVE_ALL(0); \
        call SYMBOL_NAME(timestamp_update); \
        mov $nr - 32, %rdi; \
        mov %rsp, %rsi; \
        call SYMBOL_NAME(do_irq); \
        sti; \
        jmp exit_kernel; \
    ENDPROC(isr_##nr)

#define __timer_isr(nr)

#define __apic_isr(nr) \
    ENTRY(isr_##nr) \
        cli; \
        SAVE_ALL(0); \
        call SYMBOL_NAME(timestamp_update); \
        mov $nr - 48, %rdi; \
        mov %rsp, %rsi; \
        call SYMBOL_NAME(do_irq); \
        sti; \
        jmp exit_kernel; \
    ENDPROC(isr_##nr)

#define __exception(x, nr, has_error_code, ...) \
    ENTRY(exc_##x##_handler) \
        SAVE_ALL(has_error_code); \
        call SYMBOL_NAME(timestamp_update); \
        mov $nr, %rdi; \
        mov %rsp, %rsi; \
        call do_exception; \
        jmp exit_kernel; \
    ENDPROC(exc_##x##_handler)

#define __exception_debug(x, nr, ...) \
    ENTRY(exc_##x##_handler) \
        hlt; \
    ENDPROC(exc_##x##_handler)

#endif
