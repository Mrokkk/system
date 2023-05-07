#pragma once

#include <arch/segment.h>
#include <kernel/linkage.h>

#ifdef __ASSEMBLER__

#define SECTION(sect) \
    .section sect

#define SAVE_ALL(has_error_code) \
    .if has_error_code == 0; \
        sub $4, %esp; \
    .endif; \
    pushl %gs;          /* gs */  \
    pushl %fs;          /* fs */  \
    pushl %es;          /* es */  \
    pushl %ds;          /* ds */  \
    push %eax;          /* eax */ \
    mov $KERNEL_DS, %eax;         \
    mov %ax, %ds;                 \
    mov %ax, %es;                 \
    mov %ax, %fs;                 \
    mov %ax, %ss;                 \
    mov (%esp), %eax;             \
    push %ebp;          /* ebp */ \
    push %edi;          /* edi */ \
    push %esi;          /* esi */ \
    push %edx;          /* edx */ \
    push %ecx;          /* ecx */ \
    push %ebx;          /* ebx */ \
    SAVE_USER_ESP;                \

#define SAVE_USER_ESP \
    mov REGS_CS(%esp), %edx; \
    mov SYMBOL_NAME(process_current), %ecx; \
    mov REGS_ESP(%esp), %edx; \
    mov %edx, CONTEXT_ESP2(%ecx); \

#define RESTORE_ALL \
    pop %ebx; \
    pop %ecx; \
    pop %edx; \
    pop %esi; \
    pop %edi; \
    pop %ebp; \
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
        SAVE_ALL(has_error_code); \
        call SYMBOL_NAME(timestamp_update); \
        push $nr; \
        call do_exception; \
        add $4, %esp; \
        jmp exit_kernel; \
    ENDPROC(exc_##x##_handler)

#define __exception_debug(x, nr, ...) \
    ENTRY(exc_##x##_handler) \
        SAVE_ALL(0); \
        push $nr; \
        call do_exception; \
        movl $0, %eax; \
        mov %eax, %dr6; \
        add $4, %esp; \
        jmp exit_kernel; \
        iret; \
    ENDPROC(exc_##x##_handler)

#define __syscall0(name, ...) \
    ENTRY(name) \
        mov $__NR_##name, %eax; \
        int $0x80; \
        ret; \
    ENDPROC(name)

#define __syscall1(name, ...) \
    ENTRY(name) \
        push %ebx; \
        mov $__NR_##name, %eax; \
        mov 8(%esp), %ebx; \
        int $0x80; \
        pop %ebx; \
        ret; \
    ENDPROC(name)

#define __syscall2(name, ...) \
    ENTRY(name) \
        push %ebx; \
        mov $__NR_##name, %eax; \
        mov 8(%esp), %ebx; \
        mov 12(%esp), %ecx; \
        int $0x80; \
        pop %ebx; \
        ret; \
    ENDPROC(name)

#define __syscall3(name, ...) \
    ENTRY(name) \
        push %ebx; \
        mov $__NR_##name, %eax; \
        mov 8(%esp), %ebx; \
        mov 12(%esp), %ecx; \
        mov 16(%esp), %edx; \
        int $0x80; \
        pop %ebx; \
        ret; \
    ENDPROC(name)

#define __syscall4(name, ...) \
    ENTRY(name) \
        push %ebx; \
        push %esi; \
        mov $__NR_##name, %eax; \
        mov 12(%esp), %ebx; \
        mov 16(%esp), %ecx; \
        mov 20(%esp), %edx; \
        mov 24(%esp), %esi; \
        int $0x80; \
        pop %esi; \
        pop %ebx; \
        ret; \
    ENDPROC(name)

#define __syscall5(name, ...) \
    ENTRY(name) \
        push %ebx; \
        push %esi; \
        push %edi; \
        mov $__NR_##name, %eax; \
        mov 16(%esp), %ebx; \
        mov 20(%esp), %ecx; \
        mov 24(%esp), %edx; \
        mov 28(%esp), %esi; \
        mov 32(%esp), %edi; \
        int $0x80; \
        pop %edi; \
        pop %esi; \
        pop %ebx; \
        ret; \
    ENDPROC(name)

#endif
