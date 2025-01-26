#pragma once

#ifdef __ASSEMBLER__

#define SAVE_ALL(has_error_code) \
    cld; \
    .if has_error_code == 0; \
        sub $4, %esp; \
    .endif; \
    pushl %gs;          /* gs */  \
    pushl %fs;          /* fs */  \
    pushl %es;          /* es */  \
    pushl %ds;          /* ds */  \
    push %eax;          /* eax */ \
    mov %cr2, %eax;               \
    push %eax;          /* cr2 */ \
    mov $KERNEL_DS, %eax;         \
    mov %ax, %ds;                 \
    mov %ax, %es;                 \
    mov %ax, %fs;                 \
    mov %ax, %ss;                 \
    mov 4(%esp), %eax;            \
    push %ebp;          /* ebp */ \
    push %edi;          /* edi */ \
    push %esi;          /* esi */ \
    push %edx;          /* edx */ \
    push %ecx;          /* ecx */ \
    push %ebx;          /* ebx */ \
    cmpl $KERNEL_CS, REGS_CS(%esp); \
    je 1f; \
    mov SYMBOL_NAME(process_current), %ecx; \
    mov REGS_ESP(%esp), %edx; \
    mov %edx, CONTEXT_ESP2(%ecx); \
    1:

#define RESTORE_ALL \
    pop %ebx; \
    pop %ecx; \
    pop %edx; \
    pop %esi; \
    pop %edi; \
    pop %ebp; \
    add $4, %esp; \
    pop %eax; \
    popl %ds; \
    popl %es; \
    popl %fs; \
    popl %gs; \
    add $4, %esp;

#define __pic_isr(nr) \
    ENTRY(isr_##nr) \
        cli; \
        SAVE_ALL(0); \
        call SYMBOL_NAME(timestamp_update); \
        push %esp; \
        push $nr - 32; \
        call SYMBOL_NAME(do_irq); \
        add $8, %esp; \
        sti; \
        jmp exit_kernel; \
    ENDPROC(isr_##nr)

#define __timer_isr(nr)

#define __apic_isr(nr) \
    ENTRY(isr_##nr) \
        cli; \
        SAVE_ALL(0); \
        call SYMBOL_NAME(timestamp_update); \
        push %esp; \
        push $nr - 48; \
        call SYMBOL_NAME(do_irq); \
        add $8, %esp; \
        sti; \
        jmp exit_kernel; \
    ENDPROC(isr_##nr)

#define __exception(x, nr, has_error_code, ...) \
    ENTRY(exc_##x##_handler) \
        cli; \
        SAVE_ALL(has_error_code); \
        call SYMBOL_NAME(timestamp_update); \
        push $nr; \
        call do_exception; \
        add $4, %esp; \
        sti; \
        jmp exit_kernel; \
    ENDPROC(exc_##x##_handler)

#define __exception_debug(x, nr, ...) \
    ENTRY(exc_##x##_handler) \
        cli; \
        SAVE_ALL(0); \
        push $nr; \
        call do_exception; \
        movl $0, %eax; \
        mov %eax, %dr6; \
        add $4, %esp; \
        sti; \
        jmp exit_kernel; \
    ENDPROC(exc_##x##_handler)

#endif
