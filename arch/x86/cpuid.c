#include <arch/cpuid.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/string.h>

static char* feature_strings[] = {
    "fpu",
    "vme",
    "de",
    "pse",
    "tsc",
    "msr",
    "pae",
    "mce",
    "cx8",
    "apic",
    "reserved1",
    "sep",
    "mtrr",
    "pge",
    "mca",
    "cmov",
    "pat",
    "pse_36",
    "psn",
    "clfsh",
    "reserved2",
    "ds",
    "acpi",
    "mmx",
    "fxsr",
    "sse",
    "sse2",
    "ss",
    "htt",
    "tm",
    "ia64",
    "pbe",
};

size_t cpu_features_string_get(char* buffer)
{
    uint32_t mask = 1;
    uint32_t features = cpu_info.features;
    for (uint32_t i = 0; i < 32; ++i, mask <<= 1)
    {
        if (features & mask)
        {
            buffer += sprintf(buffer, "%s ", feature_strings[i]);
        }
    }
    return 0;
}

#define L1 0
#define L2 1
#define L3 2

#define DATA_CACHE(l, s, d) \
    { \
        cpu_info.cache[l].size = s; \
        cpu_info.cache[l].description = d; \
    }

#define INSTRUCTION_CACHE(l, s, d) \
    { \
        cpu_info.instruction_cache.size = s; \
        cpu_info.instruction_cache.description = d; \
    }

#define CASE_CACHE(value, type, layer, size, description) \
    case value: \
    { \
        type(layer, size, description); \
        break; \
    }

#define CASE_CACHE_2(value, type1, layer1, size1, description1, type2, layer2, size2, description2) \
    case value: \
    { \
        type1(layer1, size1, description1); \
        type2(layer2, size2, description2); \
        break; \
    }

static void cache_info_fill(uint32_t reg)
{
    for (uint32_t i = 0; i < 4; ++i, reg >>= 8)
    {
        switch (reg & 0xff)
        {
            case 0x00:
                // Null descriptor
                break;

            case 0x01:
                // TODO
                // "Instruction TLB: 4 KiB pages, 4-way set associative, 32 entries"
                break;

            case 0x02:
                // TODO
                // "Instruction TLB: 4 MiB pages, fully associative, 2 entries"
                break;

            case 0x03:
                // TODO
                // "Data TLB: 4 KiB pages, 4-way set associative, 64 entries"
                break;

            case 0x04:
                // TODO
                // "Data TLB: 4 MiB pages, 4-way set associative, 8 entries"
                break;

            case 0x05:
                // TODO
                // "Data TLB1: 4 MiB pages, 4-way set associative, 32 entries"
                break;

            CASE_CACHE(0x06,
                INSTRUCTION_CACHE, L1, 8 * KiB,
                "1st-level instruction cache: 8 KiB, 4-way set associative, 32 byte line size");

            CASE_CACHE(0x08,
                INSTRUCTION_CACHE, L1, 16 * KiB,
                "1st-level instruction cache: 16 KiB, 4-way set associative, 32 byte line size");

            CASE_CACHE(0x09,
                INSTRUCTION_CACHE, L1, 32 * KiB,
                "1st-level instruction cache: 32 KiB, 4-way set associative, 64 byte line size");

            CASE_CACHE(0x0a,
                INSTRUCTION_CACHE, L1, 8 * KiB,
                "1st-level data cache: 8 KiB, 2-way set associative, 32 byte line size");

            case 0x0b:
                // TODO
                // "Instruction TLB: 4 MiB pages, 4-way set associative, 4 entries"
                break;

            CASE_CACHE(0x0c,
                DATA_CACHE, L1, 16 * KiB,
                "1st-level data cache: 16 KiB, 4-way set associative, 32 byte line size");

            CASE_CACHE(0x0d,
                DATA_CACHE, L1, 16 * KiB,
                "1st-level data cache: 16 KiB, 4-way set associative, 64 byte line size");

            CASE_CACHE(0x0e,
                DATA_CACHE, L1, 24 * KiB,
                "1st-level data cache: 24 KiB, 6-way set associative, 64 byte line size");

            CASE_CACHE(0x1d,
                DATA_CACHE, L2, 128 * KiB,
                "2nd-level cache: 128 KiB, 2-way set associative, 64 byte line size");

            CASE_CACHE(0x21,
                DATA_CACHE, L2, 256 * KiB,
                "2nd-level cache: 256 KiB, 8-way set associative, 64 byte line size");

            CASE_CACHE(0x22,
                DATA_CACHE, L3, 512 * KiB,
                "3rd-level cache: 512 KiB, 4-way set associative, 64 byte line size, 2 lines per sector");

            CASE_CACHE(0x23,
                DATA_CACHE, L3, 1 * MiB,
                "3rd-level cache: 1 MiB, 8-way set associative, 64 byte line size, 2 lines per sector");

            CASE_CACHE(0x24,
                DATA_CACHE, L2, 1 * MiB,
                "2nd-level cache: 1 MiB, 16-way set associative, 64 byte line size");

            CASE_CACHE(0x25,
                DATA_CACHE, L3, 2 * MiB,
                "3rd-level cache: 2 MiB, 8-way set associative, 64 byte line size, 2 lines per sector");

            CASE_CACHE(0x29,
                DATA_CACHE, L3, 4 * MiB,
                "3rd-level cache: 4 MiB, 8-way set associative, 64 byte line size, 2 lines per sector");

            CASE_CACHE(0x2c,
                DATA_CACHE, L1, 32 * KiB,
                "1st-level data cache: 32 KiB, 8-way set associative, 64 byte line size");

            CASE_CACHE(0x30,
                INSTRUCTION_CACHE, L1, 32 * KiB,
                "1st-level instruction cache: 32 KiB, 8-way set associative, 64 byte line size");

            case 0x40:
                // No 2nd-level cache or, if processor contains a valid 2nd-level cache, no 3rd-level cache
                break;

            CASE_CACHE(0x41,
                DATA_CACHE, L2, 128 * KiB,
                "2nd-level cache: 128 KiB, 4-way set associative, 32 byte line size");

            CASE_CACHE(0x42,
                DATA_CACHE, L2, 256 * KiB,
                "2nd-level cache: 256 KiB, 4-way set associative, 32 byte line size");

            CASE_CACHE(0x43,
                DATA_CACHE, L2, 512 * KiB,
                "2nd-level cache: 512 KiB, 4-way set associative, 32 byte line size");

            CASE_CACHE(0x44,
                DATA_CACHE, L2, 1 * MiB,
                "2nd-level cache: 1 MiB, 4-way set associative, 32 byte line size");

            CASE_CACHE(0x45,
                DATA_CACHE, L2, 2 * MiB,
                "2nd-level cache: 2 MiB, 4-way set associative, 32 byte line size");

            CASE_CACHE(0x46,
                DATA_CACHE, L3, 4 * MiB,
                "3rd-level cache: 4 MiB, 4-way set associative, 64 byte line size");

            CASE_CACHE(0x47,
                DATA_CACHE, L3, 3 * MiB,
                "3rd-level cache: 8 MiB, 8-way set associative, 64 byte line size");

            CASE_CACHE(0x48,
                DATA_CACHE, L2, 3 * MiB,
                "2nd-level cache: 3 MiB, 12-way set associative, 64 byte line size");

            CASE_CACHE_2(0x49,
                DATA_CACHE, L2, 4 * MiB,
                "2nd-level cache: 4 MiB, 16-way set associative, 64 byte line size",
                DATA_CACHE, L3, 4 * MiB,
                "3rd-level cache: 4 MiB, 16-way set associative, 64-byte line size");

            CASE_CACHE(0x4a,
                DATA_CACHE, L3, 6 * MiB,
                "3rd-level cache: 6 MiB, 12-way set associative, 64 byte line size");

            CASE_CACHE(0x4b,
                DATA_CACHE, L3, 8 * MiB,
                "3rd-level cache: 8 MiB, 16-way set associative, 64 byte line size");

            CASE_CACHE(0x4c,
                DATA_CACHE, L3, 12 * MiB,
                "3rd-level cache: 12 MiB, 12-way set associative, 64 byte line size");

            CASE_CACHE(0x4d,
                DATA_CACHE, L3, 16 * MiB,
                "3rd-level cache: 16 MiB, 16-way set associative, 64 byte line size");

            CASE_CACHE(0x4e,
                DATA_CACHE, L2, 6 * MiB,
                "2nd-level cache: 6 MiB, 24-way set associative, 64 byte line size");

            case 0x4f:
                // TODO
                // "Instruction TLB: 4 KiB pages, 32 entries"
                break;

            case 0x50:
                // TODO
                // "Instruction TLB: 4 KiB and 2 MiB or 4 MiB pages, 64 entries"
                break;

            case 0x51:
                // TODO
                // "Instruction TLB: 4 KiB and 2 MiB or 4 MiB pages, 128 entries"
                break;

            case 0x52:
                // TODO
                // "Instruction TLB: 4 KiB and 2 MiB or 4 MiB pages, 256 entries"
                break;

            case 0x55:
            case 0x56:
            case 0x57:
            case 0x59:
            case 0x5a:
            case 0x5b:
            case 0x5c:
            case 0x5d:

            CASE_CACHE(0x60,
                DATA_CACHE, L1, 16 * KiB,
                "1st-level data cache: 16 KiB, 8-way set associative, 64 byte line size");

            CASE_CACHE(0x7d,
                DATA_CACHE, L2, 2 * MiB,
                "2nd-level cache: 2 MiB, 8-way set associative, 64 byte line size");
        }
    }
}

static void cpu_intel()
{
    struct cpuid_regs cpuid_regs;

    sprintf(cpu_info.producer, "Intel");

    cpuid_read(1, &cpuid_regs);
    cpu_info.features = cpuid_regs.edx;

    // Check how many extended functions we have
    cpuid_read(0x80000000, &cpuid_regs);
    if (cpuid_regs.eax < 0x80000004)
    {
        strcpy(cpu_info.name, "unknown");
        goto cache_read;
    }

    cpuid_read(0x80000002, &cpuid_regs);
    memcpy(cpu_info.name, &cpuid_regs.eax, 4);
    memcpy(&cpu_info.name[4], &cpuid_regs.ebx, 4);
    memcpy(&cpu_info.name[8], &cpuid_regs.ecx, 4);
    memcpy(&cpu_info.name[12], &cpuid_regs.edx, 4);

    cpuid_read(0x80000003, &cpuid_regs);
    memcpy(&cpu_info.name[16], &cpuid_regs.eax, 4);
    memcpy(&cpu_info.name[20], &cpuid_regs.ebx, 4);
    memcpy(&cpu_info.name[24], &cpuid_regs.ecx, 4);
    memcpy(&cpu_info.name[28], &cpuid_regs.edx, 4);

    cpuid_read(0x80000004, &cpuid_regs);
    memcpy(&cpu_info.name[32], &cpuid_regs.eax, 4);
    memcpy(&cpu_info.name[36], &cpuid_regs.ebx, 4);
    memcpy(&cpu_info.name[40], &cpuid_regs.ecx, 4);
    memcpy(&cpu_info.name[44], &cpuid_regs.edx, 4);

    cpu_info.name[45] = 0;

cache_read:
    cpuid_read(2, &cpuid_regs);

    if (cpuid_regs.eax & 0xff000000)
    {
        log_warning("no valid data");
        return;
    }

    cache_info_fill(cpuid_regs.ebx);
    cache_info_fill(cpuid_regs.ecx);
    cache_info_fill(cpuid_regs.edx);

    log_info("%x", cpuid_regs.ebx);
    log_info("%x", cpuid_regs.ecx);
    log_info("%x", cpuid_regs.edx);

    for (uint32_t i = 0; i < 3; ++i)
    {
        if (!cpu_info.cache[i].description)
        {
            cpu_info.cache[i].description = "not available";
        }
    }
}

int cpu_info_get()
{
    struct cpuid_regs cpuid_regs;

    // Call CPUID function #0
    cpuid_read(0, &cpuid_regs);

    // Vendor is found in EBX, EDX, ECX in extact order
    memcpy(cpu_info.vendor, &cpuid_regs.ebx, 5);
    memcpy(&cpu_info.vendor[4], &cpuid_regs.edx, 5);
    memcpy(&cpu_info.vendor[8], &cpuid_regs.ecx, 5);
    cpu_info.vendor[12] = 0;

    switch (cpuid_regs.ebx)
    {
        // Intel CPU
        case 0x756e6547: cpu_intel(); break;
        // AMD CPU
        case 0x68747541:
        // Unknown CPU
        default: break;
    }

    return 0;
}
