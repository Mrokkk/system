#pragma once

#include <arch/register.h>
#include <arch/processor.h>

#define BIOS_VIDEO      0x10
#define BIOS_LOWMEM     0x12
#define BIOS_SYSTEM     0x15
#define BIOS_KEYBOARD   0x16

struct bios_bda
{
    uint16_t com_ports[4];
    uint16_t lpt_ports[3];
    uint16_t ebda_seg;
    uint8_t  unused[3];
    uint16_t kib_memory;
} PACKED;

typedef struct bios_bda bios_bda_t;

extern void bios_call(uint32_t function_address, regs_t* param);

static inline void* bios_ebda_get(void)
{
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105523
    extern void* bda_ptr_get(void);
    const bios_bda_t* bda = bda_ptr_get();
    const farptr_t ptr = {.seg = bda->ebda_seg, .off = 0};
    return farptr(ptr);
}
