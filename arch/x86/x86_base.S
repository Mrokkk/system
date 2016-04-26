#include <arch/x86_asm.h>
#include <arch/isr.h>
#include <arch/exception.h>
#include <arch/processor.h>
#include <arch/register.h>
#include <arch/segment.h>
#include <kernel/unistd.h>

/*===========================================================================*
 *                            __semaphore_wake                               *
 *===========================================================================*/
ENTRY(__semaphore_wake)
    push %eax
    push %edx
    push %ecx
    call __up
    pop %eax
    pop %edx
    pop %ecx
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
    pop %eax
    pop %edx
    pop %ecx
    ret
ENDPROC(__semaphore_sleep)

/*
 * Procedures copying from/to user have to
 * be here, because GCC (at least 4.8.2) can't
 * override segments.
 */

/*===========================================================================*
 *                               get_from_user                               *
 *===========================================================================*/
ENTRY(get_from_user)

        mov 4(%esp), %edx
        mov 8(%esp), %eax
        mov %gs:(%eax), %ecx
        mov %ecx, (%edx)
        ret

ENDPROC(get_from_user)

/*===========================================================================*
 *                                put_to_user                                *
 *===========================================================================*/
ENTRY(put_to_user)

        mov 4(%esp), %edx
        mov 8(%esp), %eax
        mov (%eax), %ecx
        mov %ecx, %gs:(%edx)
        ret

ENDPROC(put_to_user)

/*===========================================================================*
 *                               copy_from_user                              *
 *===========================================================================*/
ENTRY(copy_from_user)
        push %esi
        push %edi
        mov 12(%esp), %edi
        mov 16(%esp), %esi
        mov 20(%esp), %ecx
        gs
        cld
        rep
        movsb
        pop %edi
        pop %esi
        ret
ENDPROC(copy_from_user)

/*===========================================================================*
 *                                copy_to_user                               *
 *===========================================================================*/
ENTRY(copy_to_user)
        push %esi
        push %edi
        mov 12(%esp), %edi
        mov 16(%esp), %esi
        mov 20(%esp), %ecx
        push %es
        xorl %eax, %eax
        mov %gs, %eax
        mov %ax, %es
        cld
        rep
        movsb
        pop %es
        pop %edi
        pop %esi
        ret
ENDPROC(copy_to_user)

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
        mov $sse_enabled, %eax
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

        call *SYMBOL_NAME(syscalls)(, %eax, 4)
        mov %eax, REGS_EAX(%esp) /* Put value returned by *
                                  * syscall in pt_regs    */

    ENTRY(ret_from_syscall)
        RESTORE_ALL
        iret
    ENDPROC(ret_from_syscall)

ENDPROC(syscall_handler)

/*===========================================================================*
 *                              timer_handler                                *
 *===========================================================================*/
ENTRY(timer_handler) /* IRQ 0 handler */

        incl jiffies
        cmpb $0, kernel_status
        jne 1f
        push %eax
        PIC_EOI(0)
        pop %eax
        iret

    1:
        SAVE_ALL
        PIC_EOI(0)
        call SYMBOL_NAME(scheduler)
        RESTORE_ALL
        iret

ENDPROC(timer_handler)

