#include <arch/asm.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/processor.h>

#include <kernel/debug.h>
#include <kernel/api/errno.h>
#include <kernel/api/syscall.h>

#include <arch/isr.h>
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

// Return address is in EAX
ENTRY(__semaphore_sleep)
    push %eax
    push %edx
    push %ecx
    call __down_failed
    pop %ecx
    pop %edx
    ret
ENDPROC(__semaphore_sleep)

ENTRY(eip_get)
    pop %eax
    jmp *%eax
ENDPROC(eip_get)

ENTRY(vsyscall_handler)
    push %edx
    push %ecx
    push %ebx
    push %eax
    call SYMBOL_NAME(do_vsyscall)
    add $4, %esp
    pop %ebx
    pop %ecx
    pop %edx
    sysexit
ENDPROC(vsyscall_handler)

ENTRY(syscall_handler)
    SAVE_ALL(0)

    cmp $__NR_syscalls, %eax
    jge bad_syscall

    push %eax
    call trace_syscall
    mov %eax, %edi
    pop %eax

    mov SYMBOL_NAME(syscalls)(, %eax, 4), %eax
    call *%eax

    // Put value returned by syscall in pt_regs
    mov %eax, REGS_EAX(%esp)

    cmp $0, %edi
    jne 2f
1:
#if PARANOIA_SYSCALL
    call syscall_regs_check
#endif
    jmp exit_kernel
2:
    push %edi
    call trace_syscall_end
    add $4, %esp
    jmp 1b
ENDPROC(syscall_handler)

ENTRY(timer_handler)
    cli
    incl jiffies
    SAVE_ALL(0)
    call SYMBOL_NAME(timestamp_update)
    call SYMBOL_NAME(ktimers_update)
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
    jne run_scheduler

check_signals:
    mov 4(%eax), %edx
    test %edx, %edx
    jne run_signals

leave_kernel:
    RESTORE_ALL
    sti
    iret

run_scheduler:
    movl $0, (%eax)
    sti
    call SYMBOL_NAME(scheduler)

    cli
    mov SYMBOL_NAME(need_resched), %eax
    jmp check_signals

run_signals:
    push leave_kernel
    sti
    jmp SYMBOL_NAME(do_signals)

ENDPROC(exit_kernel)

ENTRY(bad_syscall)
    movl $-ENOSYS, REGS_EAX(%esp)
    jmp exit_kernel
ENDPROC(bad_syscall)
