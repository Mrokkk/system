#define log_fmt(fmt) "cpu: " fmt
#include <arch/cpuid.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/string.h>

#define FEATURE(id, name) \
    [id] = name

READONLY static const char* feature_strings[NR_FEATURES * 32] = {
    FEATURE(X86_FEATURE_FPU, "fpu"),
    FEATURE(X86_FEATURE_VME, "vme"),
    FEATURE(X86_FEATURE_DE, "de"),
    FEATURE(X86_FEATURE_PSE, "pse"),
    FEATURE(X86_FEATURE_TSC, "tsc"),
    FEATURE(X86_FEATURE_MSR, "msr"),
    FEATURE(X86_FEATURE_PAE, "pae"),
    FEATURE(X86_FEATURE_MCE, "mce"),
    FEATURE(X86_FEATURE_CX8, "cx8"),
    FEATURE(X86_FEATURE_APIC, "apic"),
    FEATURE(X86_FEATURE_RESERVED1, "reserved1"),
    FEATURE(X86_FEATURE_SEP, "sep"),
    FEATURE(X86_FEATURE_MTRR, "mtrr"),
    FEATURE(X86_FEATURE_PGE, "pge"),
    FEATURE(X86_FEATURE_MCA, "mca"),
    FEATURE(X86_FEATURE_CMOV, "cmov"),
    FEATURE(X86_FEATURE_PAT, "pat"),
    FEATURE(X86_FEATURE_PSE_36, "pse_36"),
    FEATURE(X86_FEATURE_PSN, "psn"),
    FEATURE(X86_FEATURE_CLFSH, "clfsh"),
    FEATURE(X86_FEATURE_RESERVED2, "reserved2"),
    FEATURE(X86_FEATURE_DS, "ds"),
    FEATURE(X86_FEATURE_ACPI, "acpi"),
    FEATURE(X86_FEATURE_MMX, "mmx"),
    FEATURE(X86_FEATURE_FXSR, "fxsr"),
    FEATURE(X86_FEATURE_SSE, "sse"),
    FEATURE(X86_FEATURE_SSE2, "sse2"),
    FEATURE(X86_FEATURE_SS, "ss"),
    FEATURE(X86_FEATURE_HTT, "htt"),
    FEATURE(X86_FEATURE_TM, "tm"),
    FEATURE(X86_FEATURE_IA64, "ia64"),
    FEATURE(X86_FEATURE_PBE, "pbe"),
    FEATURE(X86_FEATURE_SSE3, "sse3"),
    FEATURE(X86_FEATURE_PREFETCHW, "prefetchw"),
    FEATURE(X86_FEATURE_SYSCALL, "syscall"),
    FEATURE(X86_FEATURE_RDTSCP, "rdtscp"),
    FEATURE(X86_FEATURE_INVTSC, "invtsc"),
};

static struct cpuid_cache cache[6];

static inline const char* cache_type_string(cache_type_t t)
{
    switch (t)
    {
        case DATA: return "data";
        case INSTRUCTION: return "instruction";
        case UNIFIED: return "unified";
        default: return "unknown";
    }
}

UNMAP_AFTER_INIT static void extended_functions_read(void)
{
    cpuid_regs_t cpuid_regs = {};
    uint32_t max_function;

    snprintf(cpu_info.producer, sizeof(cpu_info.producer), "Intel");

    // Check how many extended functions we have
    cpuid_read(0x80000000, &cpuid_regs);
    max_function = cpuid_regs.eax;

    log_info("max function: %#x", max_function);

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

        cpu_info.name[48] = 0;
    }
    else
    {
        strlcpy(cpu_info.name, "unknown", sizeof(cpu_info.name));
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

#ifdef __i386__
    cpu_info.phys_bits = 32;
    cpu_info.virt_bits = 32;
#endif
}

UNMAP_AFTER_INIT int cpu_detect(void)
{
    uint32_t max_function, vendor;
    cpuid_regs_t cpuid_regs = {};

    cpuid_read(0, &cpuid_regs);

    // Vendor is found in EBX, EDX, ECX in extact order
    memcpy(cpu_info.vendor, &cpuid_regs.ebx, 4);
    memcpy(&cpu_info.vendor[4], &cpuid_regs.edx, 4);
    memcpy(&cpu_info.vendor[8], &cpuid_regs.ecx, 4);
    cpu_info.vendor[12] = 0;

    max_function = cpuid_regs.eax;
    vendor = cpuid_regs.ebx;

    if (max_function >= 1)
    {
        cpuid_read(1, &cpuid_regs);
        cpu_info.stepping = (cpuid_regs.eax) & 0xf;
        cpu_info.model = cpu_model(cpuid_regs.eax);
        cpu_info.family = cpu_family(cpuid_regs.eax);
        cpu_info.lapic_id = cpuid_regs.ebx >> 24;
        cpu_features_save(CPUID_1_ECX, cpuid_regs.ecx);
        cpu_features_save(CPUID_1_EDX, cpuid_regs.edx);
    }

    if (max_function >= 4)
    {
        for (uint32_t leaf_nr = 0; ; ++leaf_nr)
        {
            struct cpuid_cache* c = cache + leaf_nr;
            cpuid_regs.ecx = leaf_nr;
            cpuid_read(4, &cpuid_regs);

            if (!(cpuid_regs.eax & 0x1f))
            {
                break;
            }

            uint32_t eax = cpuid_regs.eax;
            uint32_t ebx = cpuid_regs.ebx;
            uint32_t ecx = cpuid_regs.ecx;

            c->layer = (eax >> 5) & 0x7;
            c->type = eax & 0x1f;
            c->line_size = (ebx & 0xfff) + 1;
            c->ways = ((ebx >> 22) & 0x3ff) + 1;
            c->partitions = ((ebx >> 12) & 0x3ff) + 1;
            c->sets = ecx + 1;
        }
    }

    cpu_info.vendor_id = vendor;

    switch (cpu_info.vendor_id)
    {
        case INTEL:
        case AMD:
            extended_functions_read();
            break;
        default:
            log_warning("unsupported CPU");
    }

    if ((cpu_info.family == 0xf && cpu_info.model >= 0x03) ||
        (cpu_info.family == 0x6 && cpu_info.model >= 0x0e))
    {
        cpu_feature_set(X86_FEATURE_INVTSC);
    }

    log_notice("producer: %s (%s), name: %s",
        cpu_info.producer,
        cpu_info.vendor,
        cpu_info.name);

    log_notice("family: %#x, model: %#x, stepping: %#x",
        cpu_info.family,
        cpu_info.model,
        cpu_info.stepping);

    for (uint32_t i = 0; i < 6; ++i)
    {
        struct cpuid_cache* c = cache + i;

        if (!c->type)
        {
            break;
        }

        log_info("L%u %s %lu KiB, %u-way associative, %u B line size",
            c->layer,
            cache_type_string(c->type),
            (c->sets * c->line_size * c->partitions * c->ways) / KiB,
            c->ways,
            c->line_size);
    }

    log_notice("features:");
    for (uint32_t i = 0; i < NR_FEATURES * 32; ++i)
    {
        if (cpu_has(i) && feature_strings[i])
        {
            log_continue(" %s", feature_strings[i]);
        }
    }

    return 0;
}
