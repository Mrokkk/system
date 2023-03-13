#include <arch/asm.h>
#include <kernel/unistd.h>

SECTION(.text)

ENTRY(_start)

    call bss_init

    call main

    push %eax
    call exit

ENDPROC(_start)
