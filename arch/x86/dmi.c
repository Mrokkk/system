#define log_fmt(fmt) "smbios: " fmt
#include <arch/dmi.h>
#include <kernel/page.h>
#include <kernel/kernel.h>

// Reference: https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.2.0.pdf

struct smbios_entry;
struct smbios_header;
typedef struct smbios_entry smbios_entry_t;
typedef struct smbios_header smbios_header_t;

struct smbios_entry
{
    char signature[4];
    uint8_t checksum;
    uint8_t len;
    uint8_t major;
    uint8_t minor;
    uint16_t max_structure_size;
    uint8_t entry_point_revision;
    char formatted_area[5];
    char signature2[5];
    uint8_t checksum2;
    uint16_t table_length;
    uint32_t table_address;
    uint16_t structs_count;
    uint8_t unused;
} PACKED;

struct smbios_header
{
    uint8_t type;
    uint8_t len;
    uint16_t handle;
} PACKED;

dmi_t dmi;
static uint32_t smbios_offset;

static smbios_entry_t* smbios_entry_find(void)
{
    int length, i;
    uint8_t checksum;

    for (uint8_t* mem = ptr(0xf0000); addr(mem) < 0x100000; mem += 16)
    {
        if (!strncmp((char*)mem, "_SM_", 4))
        {
            length = mem[5];
            checksum = 0;

            for (i = 0; i < length; i++)
            {
                checksum += mem[i];
            }

            if (checksum == 0)
            {
                return ptr(mem);
            }
        }
    }

    return NULL;
}

static size_t smbios_table_len(smbios_header_t* header)
{
    size_t i;
    const char* strtab = (const char*)header + header->len;
    // Scan until we find a double zero byte
    for (i = 1; strtab[i - 1] != '\0' || strtab[i] != '\0'; i++);
    return header->len + i + 1;
}

static inline uint8_t dmi_byte(smbios_header_t* header, int offset)
{
    return *(uint8_t*)(addr(header) + offset);
}

static inline uint16_t dmi_word(smbios_header_t* header, int offset)
{
    return *(uint16_t*)(addr(header) + offset);
}

static inline uint32_t dmi_dword(smbios_header_t* header, int offset)
{
    return *(uint32_t*)(addr(header) + offset);
}

static inline uint64_t dmi_qword(smbios_header_t* header, int offset)
{
    return *(uint64_t*)(addr(header) + offset);
}

static inline const char* dmi_string(smbios_header_t* header, int offset)
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

static inline uint32_t memory_size(smbios_header_t* header)
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

void smbios_entry_handle(smbios_header_t* header)
{
    if (header->type == 0)
    {
        uint64_t c = dmi_qword(header, 0x0a);

        log_info("BIOS: %s %s",
            dmi_string(header, 0x04),
            dmi_string(header, 0x05));

        log_continue("; start: %x; ROM size: %u KB",
            (0x10000 - dmi_word(header, 0x06)) * 16,
            64 * (dmi_byte(header, 0x09) + 1));

        if (!dmi_bit(c, 3))
        {
            log_continue("; ISA: %B; EISA: %B; PCI: %B; APM: %B; PnP: %B",
                c,
                dmi_bit(c, 4),
                dmi_bit(c, 6),
                dmi_bit(c, 7),
                dmi_bit(c, 10),
                dmi_bit(c, 9));
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

void dmi_read(void)
{
    void* mapped;
    uint32_t paddr, len;
    smbios_header_t* header, * end;
    smbios_entry_t* smbios_entry = smbios_entry_find();

    if (!smbios_entry)
    {
        log_warning("no SMBIOS");
        return;
    }

    paddr = page_beginning(smbios_entry->table_address);
    len = smbios_entry->table_length;

    mapped = region_map(paddr, len + smbios_entry->table_address - paddr, "smbios");

    if (unlikely(!mapped))
    {
        log_warning("cannot map SMBIOS region");
        return;
    }

    smbios_offset = addr(mapped) - paddr;

    header = ptr(smbios_entry->table_address + smbios_offset);
    end = ptr(addr(header) + len);

    log_notice("version: %u.%u", smbios_entry->major, smbios_entry->minor);

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
