#include <arch/asm.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/processor.h>

#include <kernel/debug.h>
#include <kernel/api/errno.h>
#include <kernel/api/syscall.h>

#include <arch/isr.h>
#include <arch/exception.h>

ENTRY(eip_get)
    pop %rax
    jmp *%rax
ENDPROC(eip_get)

ENTRY(syscall_handler)
    SAVE_ALL(0)
    call syscall
    jmp exit_kernel
ENDPROC(syscall_handler)

ENTRY(timer_handler)
    cli
    SAVE_ALL(0)
ENDPROC(timer_handler)

ENTRY(exit_kernel)
    RESTORE_ALL
    sti
    iretq
ENDPROC(exit_kernel)

ENTRY(bad_syscall)
    movq $-ENOSYS, REGS_RAX(%rsp)
    jmp exit_kernel
ENDPROC(bad_syscall)
