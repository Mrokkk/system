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
    pushl %ds
    push %edx
    push %ecx
    push %ebx
    push %eax
    movl $KERNEL_DS, %eax
    mov %ax, %ds
    call SYMBOL_NAME(do_vsyscall)
    add $4, %esp
    pop %ebx
    pop %ecx
    pop %edx
    popl %ds
    sysexit
ENDPROC(vsyscall_handler)

ENTRY(syscall_handler)
    SAVE_ALL(0)

    cmp $__NR_syscalls, %eax
    jge bad_syscall

    push %eax
    call trace_syscall
    mov %eax, %edi

    call syscall_permission_check

    cmp $0, %eax
    pop %eax
    jne 1f

    mov SYMBOL_NAME(syscalls)(, %eax, 4), %eax
    call *%eax

1:
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
    SAVE_ALL(0)
    incl jiffies
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
    mov SYMBOL_NAME(process_current), %eax
    movzbl NEED_RESCHED_SIGNAL_OFFSET(%eax), %edx
    and $1, %edx
    jne run_scheduler

check_signals:
    mov NEED_RESCHED_SIGNAL_OFFSET(%eax), %edx
    and $2, %edx
    jne run_signals

leave_kernel:
    RESTORE_ALL
    iret

run_scheduler:
    movl $0, (%eax)
    call SYMBOL_NAME(scheduler)

    mov SYMBOL_NAME(process_current), %eax
    jmp check_signals

run_signals:
    push leave_kernel
    jmp SYMBOL_NAME(do_signals)

ENDPROC(exit_kernel)

ENTRY(bad_syscall)
    movl $-ENOSYS, REGS_EAX(%esp)
    jmp exit_kernel
ENDPROC(bad_syscall)

ENTRY(empty_isr)
    iret
ENDPROC(empty_isr)

ENTRY(apic_error_isr)
    cli
    SAVE_ALL(0)
    call SYMBOL_NAME(apic_error)
    sti
    jmp exit_kernel
ENDPROC(apic_error_isr)

ENTRY(apic_spurious_isr)
    cli
    SAVE_ALL(0)
    call apic_spurious
    sti
    jmp exit_kernel
ENDPROC(apic_spurious_isr)
