#ifndef __X86_ASM_H_
#define __X86_ASM_H_

#include <kernel/linkage.h>
#include <arch/segment.h>

#define EIP_PUSHED      0
#define CS_PUSHED       4
#define EFLAGS_PUSHED   8
#define ESP_PUSHED      12
#define SS_PUSHED       16

#define SECTION(sect) \
    .section sect

#define WEAK(name) \
    .weak name; \
    name:

#define call_function0(name) \
    call SYMBOL_NAME(name)

#define call_function call_function0

#define call_function1(name, param1) \
    push param1; \
    call SYMBOL_NAME(name); \
    add $4, %esp;

#define call_function2(name, param1, param2) \
    push param2; \
    push param1; \
    call SYMBOL_NAME(name); \
    add $8, %esp;

#define call_function3(name, param1, param2, param3) \
    push param3; \
    push param2; \
    push param1; \
    call SYMBOL_NAME(name); \
    add $12, %esp;

#define call_function4(name, param1, param2, param3, param4) \
    push param4; \
    push param3; \
    push param2; \
    push param1; \
    call SYMBOL_NAME(name); \
    add $16, %esp;

#define call_function5(name, param1, param2, param3, param4, param5) \
    push param5; \
    push param4; \
    push param3; \
    push param2; \
    push param1; \
    call SYMBOL_NAME(name); \
    add $20, %esp;

#define call_function6(name, param1, param2, param3, param4, param5, param6) \
    push param6; \
    push param5; \
    push param4; \
    push param3; \
    push param2; \
    push param1; \
    call SYMBOL_NAME(name); \
    add $24, %esp;

#define SAVE_ALL \
    pushl %gs;          /* gs */  \
    pushl %fs;          /* fs */  \
    pushl %es;          /* es */  \
    pushl %ds;          /* ds */  \
    push %eax;          /* eax */ \
    mov $KERNEL_DS, %eax;         \
    mov %ax, %ds;                 \
    mov %ax, %es;                 \
    mov %ax, %fs;                 \
    mov (%esp), %eax;             \
    push %ebp;          /* ebp */ \
    push %edi;          /* edi */ \
    push %esi;          /* esi */ \
    push %edx;          /* edx */ \
    push %ecx;          /* ecx */ \
    push %ebx;          /* ebx */

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
    popl %gs;

#define PIC_EOI(nr) \
    mov $0x20, %al; \
    .if nr > 7; \
        outb %al, $0xa0; \
    .endif; \
    outb %al, $0x20

#define __pic_isr(x) \
    ENTRY(isr_##x) \
        SAVE_ALL; \
        push %esp; \
        push $x-0x20; \
        call SYMBOL_NAME(do_irq); \
        PIC_EOI(x); \
        add $8, %esp; \
        RESTORE_ALL; \
        iret; \
    ENDPROC(isr_##x)

#define __timer_isr __pic_isr

#define __exception_noerrno(x) \
    ENTRY(exc_##x##_handler) \
        SAVE_ALL; \
        push $0; \
        push $__NR_##x; \
        call do_exception; \
        add $12, %esp; \
        RESTORE_ALL; \
        iret; \
    ENDPROC(exc_##x##_handler)

#define __exception_errno(x) \
    ENTRY(exc_##x##_handler) \
        mov (%esp), %eax; \
        add $4, %esp; \
        SAVE_ALL; \
        push %eax; \
        push $__NR_##x; \
        call do_exception; \
        add $12, %esp; \
        RESTORE_ALL; \
        iret; \
    ENDPROC(exc_##x##_handler)

#define __exception_debug(x) \
    ENTRY(exc_##x##_handler) \
        SAVE_ALL; \
        push $0; \
        push $__NR_##x; \
        call do_exception; \
        movl $0, %eax; \
        mov %eax, %dr6; \
        add $12, %esp; \
        RESTORE_ALL; \
        iret; \
    ENDPROC(exc_##x##_handler)

#define __syscall0(name) \
    ENTRY(name) \
        mov $__NR_##name, %eax; \
        int $0x80; \
        ret; \
    ENDPROC(name)

#define __syscall1(name, type1) \
    ENTRY(name) \
        mov $__NR_##name, %eax; \
        mov 4(%esp), %ebx; \
        int $0x80; \
        ret; \
    ENDPROC(name)

#define __syscall2(name, type1, type2) \
    ENTRY(name) \
        push %ebx; \
        mov $__NR_##name, %eax; \
        mov 8(%esp), %ebx; \
        mov 12(%esp), %ecx; \
        int $0x80; \
        pop %ebx; \
        ret; \
    ENDPROC(name)

#define __syscall3(name, type1, type2, type3) \
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

#define __syscall4(name, type1, type2, type3, type4) \
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

#define __syscall5(name, type1, type2, type3, type4, type5) \
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

#define __breakpoint xchgw %bx, %bx

#define printd(p1, p2, p3) \
    push p3; \
    push p2; \
    push p1; \
    call printk; \
    add $12, %esp;

#define __debugpoint \
    pusha; \
    push %esi; \
    push %edi; \
    push %ebp; \
    push %edx; \
    push %ecx; \
    push %ebx; \
    printd($1f, $2f, %eax); \
    pop %ebx; \
    printd($1f, $3f, %ebx); \
    pop %ecx; \
    printd($1f, $4f, %ecx); \
    pop %edx; \
    printd($1f, $5f, %edx); \
    push $14f; \
    call printk; \
    add $4, %esp; \
    pop %ebp; \
    printd($1f, $6f, %ebp); \
    pop %edi; \
    printd($1f, $7f, %edi); \
    pop %esi; \
    printd($1f, $8f, %esi); \
    printd($1f, $9f, %esp); \
    push $14f; \
    call printk; \
    add $4, %esp; \
    pushf; \
    pop %eax; \
    printd($1f, $10f, %eax); \
    mov %cr0, %eax; \
    printd($1f, $11f, %eax); \
    mov %cr3, %eax; \
    printd($1f, $13f, %eax); \
    push $14f; \
    call printk; \
    add $4, %esp; \
    popa; \
    jmp 15f; \
    1: .asciz "%s = 0x%x; "; \
    2: .asciz "EAX"; \
    3: .asciz "EBX"; \
    4: .asciz "ECX"; \
    5: .asciz "EDX"; \
    6: .asciz "EBP"; \
    7: .asciz "EDI"; \
    8: .asciz "ESI"; \
    9: .asciz "ESP"; \
    10: .asciz "EFLAGS"; \
    11: .asciz "CR0"; \
    12: .asciz "CR1"; \
    13: .asciz "CR3"; \
    14: .asciz "\n"; \
    15:

#endif /* __X86_ASM_H_ */
