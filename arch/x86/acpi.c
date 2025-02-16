#define log_fmt(fmt) "acpi: " fmt
#include <arch/io.h>
#include <arch/acpi.h>
#include <arch/bios.h>
#include <arch/bios32.h>
#include <arch/reboot.h>
#include <kernel/kernel.h>
#include <kernel/page_mmio.h>

#define DEBUG_RSDT      0
#define RSDP_SIGNATURE  U32('R', 'S', 'D', ' ')

READONLY static rsdt_t* rsdt;
READONLY static size_t rsdt_count;
READONLY static int acpi_offset;
READONLY static uint16_t reset_port;
READONLY static uint8_t reset_value;

static void reboot_by_acpi(void)
{
    outb(reset_value, reset_port);
}

UNMAP_AFTER_INIT void acpi_initialize(void)
{
    uint32_t paddr, start, end;
    void* mapped;
    fadt_t* fadt = NULL;
    sdt_header_t* header;
    rsdp_descriptor_t* rsdp = bios_find(RSDP_SIGNATURE);

    if (!rsdp)
    {
        log_notice("no RSDP found");
        return;
    }

    paddr = page_beginning(rsdp->rsdt_addr);

    // Map initial RSDT area so root SDT can be read
    mapped = mmio_map_uc(paddr, PAGE_SIZE, "acpi");

    if (!mapped)
    {
        log_warning("cannot map APCI region");
        return;
    }

    acpi_offset = addr(mapped) - paddr;
    rsdt = ptr(rsdp->rsdt_addr + acpi_offset);

    header = &rsdt->header;
    rsdt_count = (header->len - sizeof(sdt_header_t)) / sizeof(uint32_t);

    log_notice("oem: \"%.6s\" revision: %#x rsdt = %#x", rsdp->oemid, rsdp->revision, rsdp->rsdt_addr);
    log_notice("root: %.4s len: %u entries: %zu", header->signature, header->len, rsdt_count);

    start = paddr, end = paddr + PAGE_SIZE;

    // Get whole range of RSDT
    for (uint32_t i = 0; i < rsdt_count; ++i)
    {
        if (rsdt->table[i] < start)
        {
            start = page_beginning(rsdt->table[i]);
        }
        else if (rsdt->table[i] >= end)
        {
            end = page_align(rsdt->table[i]);
        }
    }

    log_notice("[mem %#010x - %#010x %#x]", start, end - 1, end - start);

    // If range exceeds already mapped PAGE_SIZE, remap
    if (end - start > PAGE_SIZE)
    {
        mapped = mmio_map_uc(start, end - start, "acpi_refined");

        acpi_offset = addr(mapped) - start;
        rsdt = ptr(rsdp->rsdt_addr + acpi_offset);
    }

    for (uint32_t i = 0; i < rsdt_count; ++i)
    {
        header = ptr(rsdt->table[i] + acpi_offset);
        if (DEBUG_RSDT)
        {
            log_notice("%#x %.4s len: %u", rsdt->table[i], header->signature, header->len);
        }

        if (!strncmp(header->signature, "FACP", 4))
        {
            fadt = ptr(header);
        }
    }

    if (!fadt)
    {
        return;
    }

    static_assert(offsetof(fadt_t, reset_req) == 116, "Wrong offset of reset_req");

    log_notice("fadt: version: %#x", fadt->header.revision);
    log_notice("fadt: facs: %#x", fadt->firmware_ctrl);
    log_notice("fadt: SCI interrupt: %#x", fadt->sci_int);
    log_notice("fadt: PM timer: %#x", fadt->pm_tmr_blk);

    if (fadt->header.revision >= 2)
    {
        log_notice("legacy devices: %s", fadt->iapc_boot_arch & IAPC_BOOT_LEGACY
            ? "supported"
            : "unsupported");
        log_notice("8042: %s", fadt->iapc_boot_arch & IAPC_BOOT_8042
            ? "supported"
            : "unsupported");
        log_notice("VGA: %s", !(fadt->iapc_boot_arch & IAPC_BOOT_NO_VGA)
            ? "supported"
            : "unsupported");

        log_notice("reset req: space: %#x, access_size: %#x, addr: %#llx, value: %#x",
            fadt->reset_req.address_space, fadt->reset_req.access_size,
            fadt->reset_req.address, fadt->reset_value);

        if (fadt->reset_req.address_space == 0x1)
        {
            reset_port = fadt->reset_req.address;
            reset_value = fadt->reset_value;
            reboot_fn = &reboot_by_acpi;
        }
    }
}

void* acpi_find(char* signature)
{
    for (uint32_t i = 0; i < rsdt_count; ++i)
    {
        sdt_header_t* h = ptr(rsdt->table[i] + acpi_offset);
        if (!strncmp(h->signature, signature, 4))
        {
            return h;
        }
    }
    return NULL;
}
