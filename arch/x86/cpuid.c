#include <arch/cpuid.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/string.h>

#define FEATURE(id, name) \
    [id] = name

static char* feature_strings[] = {
    FEATURE(INTEL_FPU, "fpu"),
    FEATURE(INTEL_VME, "vme"),
    FEATURE(INTEL_DE, "de"),
    FEATURE(INTEL_PSE, "pse"),
    FEATURE(INTEL_TSC, "tsc"),
    FEATURE(INTEL_MSR, "msr"),
    FEATURE(INTEL_PAE, "pae"),
    FEATURE(INTEL_MCE, "mce"),
    FEATURE(INTEL_CX8, "cx8"),
    FEATURE(INTEL_APIC, "apic"),
    FEATURE(INTEL_RESERVED1, "reserved1"),
    FEATURE(INTEL_SEP, "sep"),
    FEATURE(INTEL_MTRR, "mtrr"),
    FEATURE(INTEL_PGE, "pge"),
    FEATURE(INTEL_MCA, "mca"),
    FEATURE(INTEL_CMOV, "cmov"),
    FEATURE(INTEL_PAT, "pat"),
    FEATURE(INTEL_PSE_36, "pse_36"),
    FEATURE(INTEL_PSN, "psn"),
    FEATURE(INTEL_CLFSH, "clfsh"),
    FEATURE(INTEL_RESERVED2, "reserved2"),
    FEATURE(INTEL_DS, "ds"),
    FEATURE(INTEL_ACPI, "acpi"),
    FEATURE(INTEL_MMX, "mmx"),
    FEATURE(INTEL_FXSR, "fxsr"),
    FEATURE(INTEL_SSE, "sse"),
    FEATURE(INTEL_SSE2, "sse2"),
    FEATURE(INTEL_SS, "ss"),
    FEATURE(INTEL_HTT, "htt"),
    FEATURE(INTEL_TM, "tm"),
    FEATURE(INTEL_IA64, "ia64"),
    FEATURE(INTEL_PBE, "pbe"),
    FEATURE(INTEL_SSE3, "sse3"),
    FEATURE(INTEL_PREFETCHW, "prefetchw"),
    FEATURE(INTEL_SYSCALL, "syscall"),
    FEATURE(INTEL_RDTSCP, "rdtscp"),
    FEATURE(INTEL_INVTSC, "invtsc"),
};

size_t cpu_features_string_get(char* buffer)
{
    for (uint32_t i = 0; i < NR_FEATURES * 32; ++i)
    {
        if (cpu_has(i) && feature_strings[i])
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
    uint32_t max_function;

    sprintf(cpu_info.producer, "Intel");

    // Check how many extended functions we have
    cpuid_read(0x80000000, &cpuid_regs);
    max_function = cpuid_regs.eax;

    if (max_function >= 0x80000001)
    {
        cpuid_read(0x80000001, &cpuid_regs);
        cpu_features_save(CPUID_80000001, cpuid_regs.edx);
    }

    if (max_function >= 0x80000004)
    {
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
    }
    else
    {
        strcpy(cpu_info.name, "unknown");
    }

    if (max_function >= 0x80000006)
    {
        cpuid_read(0x80000006, &cpuid_regs);
        cpu_info.cacheline_size = cpuid_regs.ecx & 0xff;
        cpu_info.cache_size = (cpuid_regs.ecx >> 16) * KiB;
    }

    if (max_function >= 0x80000007)
    {
        cpuid_read(0x80000007, &cpuid_regs);
        cpu_features_save(CPUID_80000007, cpuid_regs.edx);
    }
}

int cpu_info_get()
{
    struct cpuid_regs cpuid_regs;

    cpuid_read(1, &cpuid_regs);
    cpu_info.stepping = (cpuid_regs.eax) && 0xf;
    cpu_info.model = (cpuid_regs.eax >>= 4) && 0xf;
    cpu_info.family = (cpuid_regs.eax >>= 4) && 0xf;
    cpu_features_save(CPUID_1_ECX, cpuid_regs.ecx);
    cpu_features_save(CPUID_1_EDX, cpuid_regs.edx);

    cpuid_read(2, &cpuid_regs);

    if (cpuid_regs.eax & 0xff000000)
    {
        log_warning("no valid cache data!");
        goto cont;
    }

    cache_info_fill(cpuid_regs.ebx);
    cache_info_fill(cpuid_regs.ecx);
    cache_info_fill(cpuid_regs.edx);

cont:
    for (uint32_t i = 0; i < 3; ++i)
    {
        if (!cpu_info.cache[i].description)
        {
            cpu_info.cache[i].description = "not available";
        }
    }

    cpuid_read(0, &cpuid_regs);

    // Vendor is found in EBX, EDX, ECX in extact order
    memcpy(cpu_info.vendor, &cpuid_regs.ebx, 4);
    memcpy(&cpu_info.vendor[4], &cpuid_regs.edx, 4);
    memcpy(&cpu_info.vendor[8], &cpuid_regs.ecx, 4);
    cpu_info.vendor[12] = 0;

    switch (cpuid_regs.ebx)
    {
        // Intel CPU - "Genu"
        case 0x756e6547: cpu_intel(); break;
        // AMD CPU
        case 0x68747541:
        // Unknown CPU
        default:
    }

    return 0;
}
