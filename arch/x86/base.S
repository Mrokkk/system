#include <arch/asm.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/processor.h>

#include <kernel/errno.h>

#include <arch/isr.h>
#include <kernel/unistd.h>
#include <arch/exception.h>

ENTRY(__semaphore_wake)
    push %eax
    push %edx
    push %ecx
    call __up
    pop %ecx
    pop %edx
    pop %eax
    ret
ENDPROC(__semaphore_wake)

ENTRY(__semaphore_sleep)
    push %eax
    push %edx
    push %ecx
    call __down_failed
    pop %ecx
    pop %edx
    pop %eax
    ret
ENDPROC(__semaphore_sleep)

ENTRY(eip_get)
    pop %eax
    jmp *%eax
ENDPROC(eip_get)

ENTRY(syscall_handler)
    SAVE_ALL(0)
    cmp $__NR_syscalls, %eax
    jg bad_syscall
    mov SYMBOL_NAME(syscalls)(, %eax, 4), %eax
    cmp $0, %eax
    jz bad_syscall
    call *%eax
    mov %eax, REGS_EAX(%esp) // Put value returned by syscall in pt_regs
    call syscall_regs_check
    jmp exit_kernel
ENDPROC(syscall_handler)

ENTRY(timer_handler)
    cli
    incl jiffies
    SAVE_ALL(0)
    call SYMBOL_NAME(timestamp_update)
    push $0
    call SYMBOL_NAME(irq_eoi)
    sti
    call SYMBOL_NAME(scheduler)
    add $4, %esp
ENDPROC(timer_handler)

ENTRY(exit_kernel)
    cli
    mov SYMBOL_NAME(need_resched), %eax
    mov (%eax), %edx
    test %edx, %edx
    jne 1f
    RESTORE_ALL
    sti
    iret
1:
    movl $0, (%eax)
    sti
    call SYMBOL_NAME(scheduler)
    RESTORE_ALL
    iret
ENDPROC(exit_kernel)

ENTRY(bad_syscall)
    movl $-ENOSYS, REGS_EAX(%esp)
    jmp exit_kernel
ENDPROC(bad_syscall)
