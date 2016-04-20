#include <arch/x86_asm.h>

.code16

/*===========================================================================*
 *                                bios_int10h                                *
 *===========================================================================*/
ENTRY(bios_int10h)
    int $0x10
    ret
ENDPROC(bios_int10h)

ENTRY(bios_int15h)
    int $0x15
    ret
ENDPROC(bios_int15h)

/* Size of above procedure */
ENTRY(bios_int10h_size)
    .long .-bios_int10h
