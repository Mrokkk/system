#define log_fmt(fmt) "bios32: " fmt
#include <arch/bios32.h>
#include <arch/register.h>

#include <kernel/kernel.h>
#include <kernel/page_alloc.h>

READONLY static bios32_header_t* bios32_header;
READONLY static bios32_entry_t entry;

UNMAP_AFTER_INIT int bios32_init(void)
{
    for (uint32_t* lowmem = (uint32_t*)0xe0000; lowmem < (uint32_t*)0x100000; ++lowmem)
    {
        if (*lowmem == BIOS32_SIGNATURE)
        {
            bios32_header = ptr(lowmem);

            log_notice("base: %lx; entry: %lx", bios32_header, bios32_header->entry);

            entry.addr = bios32_header->entry;
            entry.seg = KERNEL_CS;
            return 0;
        }
    }
    return -ENOSYS;
}

int bios32_find(uint32_t service, bios32_entry_t* service_entry)
{
    regs_t regs = {};

    if (!bios32_header)
    {
        return -ENOSYS;
    }

    regs.eax = service;
    bios32_call(&entry, &regs);

    if (regs.al)
    {
        return -ENOSYS;
    }

    service_entry->addr = regs.ebx + regs.edx;
    service_entry->seg = KERNEL_CS;

    return 0;
}
