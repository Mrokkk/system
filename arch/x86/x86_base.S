#include <arch/x86_asm.h>
#include <arch/isr.h>
#include <arch/exception.h>
#include <arch/processor.h>
#include <arch/register.h>
#include <arch/segment.h>
#include <kernel/unistd.h>
#include <kernel/errno.h>

/*===========================================================================*
 *                            __semaphore_wake                               *
 *===========================================================================*/
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

/*===========================================================================*
 *                            __semaphore_sleep                              *
 *===========================================================================*/
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

/*===========================================================================*
 *                                sse_enable                                 *
 *===========================================================================*/
ENTRY(sse_enable)
        mov %cr0, %eax
        and $0xfffb, %ax
        or $0x2, %ax
        mov %eax, %cr0
        mov %cr4, %eax
        or $(3 << 9), %ax
        mov %eax, %cr4
        movb $1, (%eax)
        ret
ENDPROC(sse_enable)

/*===========================================================================*
 *                                 do_delay                                  *
 *===========================================================================*/
ENTRY(do_delay)
        mov 4(%esp), %eax
        jmp 1f
    .align 16
    1:  jmp 2f
    .align 16
    2:  decl %eax
        jns 2b
        ret
ENDPROC(do_delay)

/*===========================================================================*
 *                                 eip_get                                   *
 *===========================================================================*/
ENTRY(eip_get)
        pop %eax
        jmp *%eax
ENDPROC(eip_get)

/*===========================================================================*
 *                              syscall_handler                              *
 *===========================================================================*/
ENTRY(syscall_handler) /* INT 0x80 handler */

        SAVE_ALL

        cmp $__NR_syscalls, %eax
        jg bad_syscall
        mov SYMBOL_NAME(syscalls)(, %eax, 4), %ebx
        cmp $0, %ebx
        jz bad_syscall
        call *%ebx
        mov %eax, REGS_EAX(%esp) /* Put value returned by *
                                  * syscall in pt_regs    */
ENDPROC(syscall_handler)

ENTRY(ret_from_syscall)
        RESTORE_ALL
        iret
ENDPROC(ret_from_syscall)

bad_syscall:
    movl $-ENOSYS, REGS_EAX(%esp)
    jmp ret_from_syscall

/*===========================================================================*
 *                              timer_handler                                *
 *===========================================================================*/
ENTRY(timer_handler) /* IRQ 0 handler */

        //1:
        //jmp 1b
        incl jiffies
        SAVE_ALL
        PIC_EOI(0)
        call SYMBOL_NAME(scheduler)
        RESTORE_ALL
        iret

ENDPROC(timer_handler)

