#define log_fmt(fmt) "bios32: " fmt
#include <arch/bios.h>
#include <arch/bios32.h>
#include <arch/register.h>

#include <kernel/kernel.h>
#include <kernel/page_alloc.h>

READONLY static bios32_header_t* bios32_header;
READONLY static bios32_entry_t entry;

UNMAP_AFTER_INIT int bios32_init(void)
{
    bios32_header = bios_find(BIOS32_SIGNATURE);

    if (!bios32_header)
    {
        return -ENOSYS;
    }

    log_notice("base: %p; entry: %p", bios32_header, ptr(bios32_header->entry));

    entry.addr = bios32_header->entry;
    entry.seg = KERNEL_CS;

    return 0;
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
