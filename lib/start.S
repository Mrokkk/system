#include <arch/asm.h>

SECTION(.text)

ENTRY(_start)

    call bss_init

    call main

    push %eax
    call exit

ENDPROC(_start)
