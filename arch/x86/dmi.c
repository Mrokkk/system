#define log_fmt(fmt) "dmi: " fmt
#include <arch/io.h>
#include <arch/dmi.h>
#include <kernel/kernel.h>
#include <kernel/page_mmio.h>

// Reference: https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.2.0.pdf

typedef struct smbios_entry smbios_entry_t;
typedef struct dmi_header dmi_header_t;
typedef struct dmi_legacy_entry dmi_legacy_entry_t;

static uintptr_t smbios_dmi_handle(void* addr, size_t* size);
static uintptr_t dmi_legacy_handle(void* addr, size_t* size);

struct smbios_entry
{
    char     signature[4];
    uint8_t  checksum;
    uint8_t  len;
    uint8_t  major;
    uint8_t  minor;
    uint16_t max_structure_size;
    uint8_t  entry_point_revision;
    char     formatted_area[5];
    char     signature2[5];
    uint8_t  checksum2;
    uint16_t table_len;
    uint32_t table_addr;
    uint16_t structs_count;
    uint8_t  unused;
} PACKED;

struct dmi_legacy_entry
{
    char     signature[5];
    char     checksum;
    uint16_t table_len;
    uint32_t table_addr;
    uint16_t structs_count;
    uint8_t  version;
} PACKED;

struct dmi_header
{
    uint8_t  type;
    uint8_t  len;
    uint16_t handle;
} PACKED;

struct dmi_entry_parser
{
    const char* signature;
    uintptr_t (*table_get)(void* addr, size_t* size);
};

typedef struct dmi_entry_parser dmi_entry_parser_t;

dmi_t dmi;

static dmi_entry_parser_t parsers[] = {
    {
        .signature = "_SM_",
        .table_get = &smbios_dmi_handle,
    },
    {
        .signature = "_DMI_",
        .table_get = &dmi_legacy_handle,
    },
};

static uintptr_t smbios_dmi_handle(void* addr, size_t* size)
{
    smbios_entry_t* smbios_entry = addr;

    uint8_t checksum = 0;

    for (uint32_t i = 0; i < smbios_entry->len; i++)
    {
        checksum += readb(addr + i);
    }

    if (checksum != 0)
    {
        log_notice("SMBIOS entry @ %p: bad checksum: %#x", addr, checksum);
        return 0;
    }

    log_notice("SMBIOS %u.%u present", smbios_entry->major, smbios_entry->minor);

    *size = smbios_entry->table_len;
    return smbios_entry->table_addr;
}

static uintptr_t dmi_legacy_handle(void* addr, size_t* size)
{
    dmi_legacy_entry_t* dmi_entry = addr;

    if (!dmi_entry->table_addr || !dmi_entry->table_len)
    {
        return 0;
    }

    log_notice("Legacy DMI %u.%u present", dmi_entry->version >> 4, dmi_entry->version & 0xf);

    *size = dmi_entry->table_len;
    return dmi_entry->table_addr;
}

static uintptr_t dmi_table_find(size_t* table_len)
{
    for (uint8_t* mem = ptr(0xf0000); addr(mem) < 0x100000; mem += 16)
    {
        for (size_t i = 0; i < array_size(parsers); ++i)
        {
            dmi_entry_parser_t* p = &parsers[i];
            if (!strncmp((char*)mem, p->signature, strlen(p->signature)))
            {
                return p->table_get(mem, table_len);
            }
        }
    }

    return 0;
}

static size_t smbios_table_len(dmi_header_t* header)
{
    size_t i;
    const char* strtab = (const char*)header + header->len;
    // Scan until we find a double zero byte
    for (i = 1; strtab[i - 1] != '\0' || strtab[i] != '\0'; i++);
    return header->len + i + 1;
}

static inline uint8_t dmi_byte(dmi_header_t* header, int offset)
{
    return *(uint8_t*)(addr(header) + offset);
}

static inline uint16_t dmi_word(dmi_header_t* header, int offset)
{
    return *(uint16_t*)(addr(header) + offset);
}

static inline uint32_t dmi_dword(dmi_header_t* header, int offset)
{
    return *(uint32_t*)(addr(header) + offset);
}

static inline uint64_t dmi_qword(dmi_header_t* header, int offset)
{
    return *(uint64_t*)(addr(header) + offset);
}

static inline const char* dmi_string(dmi_header_t* header, int offset)
{
    const char* strings;
    uint32_t index = dmi_byte(header, offset);

    if (index == 0)
    {
        return "<empty>";
    }

    strings = ptr(addr(header) + header->len);

    index--;

    while (index)
    {
        if (*strings++ == 0)
        {
            index--;
        }
    }

    return strings;
}

#define dmi_bit(v, bit) ({ (int)((v) & (1 << bit)); })

static inline uint32_t memory_size(dmi_header_t* header)
{
    uint16_t size = dmi_word(header, 0x0c);

    if (size & (1 << 15))
    {
        return 0;
    }
    else if (size == 0x7fff)
    {
        return dmi_dword(header, 0x1c);
    }

    return size;
}

static inline const char* memory_form_factor(uint8_t f)
{
    switch (f)
    {
        case 0x09:  return "DIMM";
        case 0x0d:  return "SODIMM";
        default:    return "Unknown";
    }
}

static inline const char* memory_type(uint8_t t)
{
    switch (t)
    {
        case 0x07:  return "RAM";
        case 0x12:  return "DDR";
        case 0x13:  return "DDR2";
        case 0x18:  return "DDR3";
        case 0x1a:  return "DDR4";
        default:    return "Unknown";
    }
}

UNMAP_AFTER_INIT static void smbios_entry_handle(dmi_header_t* header)
{
    if (header->type == 0)
    {
        uint64_t c = dmi_qword(header, 0x0a);

        if (!dmi.bios)
        {
            dmi.bios = dmi_string(header, 0x04);
            dmi.bios_version = dmi_string(header, 0x05);

            log_info("BIOS: %s %s",
                dmi.bios,
                dmi.bios_version);

            log_continue("; start: %#x; ROM size: %u KB",
                (0x10000 - dmi_word(header, 0x06)) * 16,
                64 * (dmi_byte(header, 0x09) + 1));

            if (!dmi_bit(c, 3))
            {
                log_continue("; ISA: %B; EISA: %B; PCI: %B; APM: %B; PnP: %B",
                    dmi_bit(c, 4),
                    dmi_bit(c, 6),
                    dmi_bit(c, 7),
                    dmi_bit(c, 10),
                    dmi_bit(c, 9));
            }
        }
    }
    else if (header->type == 1)
    {
        dmi.manufacturer = dmi_string(header, 0x04);
        dmi.product = dmi_string(header, 0x05);
        dmi.version = dmi_string(header, 0x06);

        log_info("System Info: manufacturer: %.64s; product: %.64s; version: %.64s",
            dmi.manufacturer,
            dmi.product,
            dmi.version);
    }
    else if (header->type == 4)
    {
        log_info("Processor: manufacturer: %.64s; product: %.64s; max: %u MHz; current %u MHz; external: %u MHz",
            dmi_string(header, 0x07),
            dmi_string(header, 0x10),
            dmi_word(header, 0x14),
            dmi_word(header, 0x16),
            dmi_word(header, 0x12));
    }
    else if (header->type == 17)
    {
        uint8_t form_factor = dmi_byte(header, 0x0e);
        uint8_t type = dmi_byte(header, 0x12);
        log_info("Memory Device: %s %s %.64s; size: %u MiB; speed: %u MT/s",
            memory_type(type),
            memory_form_factor(form_factor),
            dmi_string(header, 0x11),
            memory_size(header),
            dmi_word(header, 0x15));
    }
}

UNMAP_AFTER_INIT void dmi_read(void)
{
    size_t table_len;
    dmi_header_t* end;
    dmi_header_t* header;
    uintptr_t table_addr = dmi_table_find(&table_len);

    if (!table_addr)
    {
        log_notice("no DMI information");
        return;
    }

    uintptr_t paddr = page_beginning(table_addr);
    uintptr_t offset = table_addr & PAGE_MASK;

    void* mapped = mmio_map_uc(paddr, table_len + table_addr - paddr, "dmi");

    if (unlikely(!mapped))
    {
        log_warning("cannot map DMI table");
        return;
    }

    header = mapped + offset;
    end = ptr(addr(header) + table_len);

    for (; header < end;)
    {
        smbios_entry_handle(header);
        header = ptr(addr(header) + smbios_table_len(header));
        if (header->type == 127)
        {
            break;
        }
    }
}
