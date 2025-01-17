#include <arch/vbe.h>
#include <arch/bios.h>

#include <kernel/vesa.h>

int vbe_call(vbe_regs_t* regs)
{
    bios_call(BIOS_VIDEO, regs);
    return regs->ax == VBE_SUPPORTED ? 0 : -1;
}
