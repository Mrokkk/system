#define log_fmt(fmt) "cpu: " fmt
#include <arch/cpuid.h>
#include <arch/percpu.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/string.h>

// References:
// https://datasheets.chipdb.org/Intel/x86/CPUID/24161821.pdf
// https://datasheets.chipdb.org/AMD/486_5x86/19720C.pdf
// https://www.ardent-tool.com/CPU/docs/AMD/recognition/20734R.pdf

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
int cpuid_available;
int family;

static inline const char* cache_type_string(cache_type_t t)
{
    switch (t)
    {
        case DATA:        return "data";
        case INSTRUCTION: return "instruction";
        case UNIFIED:     return "unified";
        default:          return "unknown";
    }
}

#define NAME_APPEND(str) \
    it = csnprintf(it, end, str)

UNMAP_AFTER_INIT static void extended_functions_read(cpu_info_t* this_cpu)
{
    cpuid_regs_t cpuid_regs = {};
    uint32_t max_function;

    // Check how many extended functions we have
    cpuid_read(0x80000000, &cpuid_regs);
    max_function = cpuid_regs.eax;

    this_cpu->max_extended_function = max_function;

    if (max_function >= 0x80000001)
    {
        cpuid_read(0x80000001, &cpuid_regs);
        cpu_features_save(CPUID_80000001, cpuid_regs.edx);
    }

    if (max_function >= 0x80000004)
    {
        cpuid_read(0x80000002, &cpuid_regs);
        memcpy(this_cpu->name, &cpuid_regs.eax, 4);
        memcpy(&this_cpu->name[4], &cpuid_regs.ebx, 4);
        memcpy(&this_cpu->name[8], &cpuid_regs.ecx, 4);
        memcpy(&this_cpu->name[12], &cpuid_regs.edx, 4);

        cpuid_read(0x80000003, &cpuid_regs);
        memcpy(&this_cpu->name[16], &cpuid_regs.eax, 4);
        memcpy(&this_cpu->name[20], &cpuid_regs.ebx, 4);
        memcpy(&this_cpu->name[24], &cpuid_regs.ecx, 4);
        memcpy(&this_cpu->name[28], &cpuid_regs.edx, 4);

        cpuid_read(0x80000004, &cpuid_regs);
        memcpy(&this_cpu->name[32], &cpuid_regs.eax, 4);
        memcpy(&this_cpu->name[36], &cpuid_regs.ebx, 4);
        memcpy(&this_cpu->name[40], &cpuid_regs.ecx, 4);
        memcpy(&this_cpu->name[44], &cpuid_regs.edx, 4);

        this_cpu->name[48] = 0;
    }
    else
    {
        char* it = this_cpu->name;
        const char* end = this_cpu->name + sizeof(this_cpu->name);
        switch (this_cpu->vendor_id)
        {
            case INTEL:
                NAME_APPEND("Intel ");
                switch (this_cpu->family)
                {
                    case 4:
                        switch (this_cpu->model)
                        {
                            case 0:
                            case 1:
                                NAME_APPEND("i486DX");
                                break;
                            case 2:
                                NAME_APPEND("i486SX");
                                break;
                            case 3:
                                NAME_APPEND("iDX2");
                                break;
                            case 4:
                                NAME_APPEND("i486 SL");
                                break;
                            case 5:
                                NAME_APPEND("iSX2");
                                break;
                            case 7:
                                NAME_APPEND("iDX2 WB");
                                break;
                            case 8:
                                NAME_APPEND("iDX4");
                                break;
                            default:
                                goto unknown;
                        }
                        break;
                    case 5:
                        switch (this_cpu->model)
                        {
                            case 1:
                                NAME_APPEND("Pentium");
                                break;
                            case 3:
                                NAME_APPEND("Pentium OverDrive");
                                break;
                            case 4:
                                NAME_APPEND("Pentium MMX");
                                break;
                            default:
                                goto unknown;
                        }
                        break;
                    case 6:
                        switch (this_cpu->model)
                        {
                            case 1:
                                NAME_APPEND("Pentium Pro");
                                break;
                            case 3:
                            case 5:
                                NAME_APPEND("Pentium II");
                                break;
                            case 6:
                                NAME_APPEND("Celeron");
                                break;
                            case 7:
                                NAME_APPEND("Pentium III");
                                break;
                            default:
                                goto unknown;
                        }
                        break;
                    default:
                        goto unknown;
                }
                if (this_cpu->type == 1)
                {
                    NAME_APPEND(" OverDrive");
                }
                break;
            case AMD:
                NAME_APPEND("AMD ");
                switch (this_cpu->family)
                {
                    case 4:
                        switch (this_cpu->model)
                        {
                            case 3:
                                NAME_APPEND("Am486DX2-WT");
                                break;
                            case 7:
                                NAME_APPEND("Am486DX2-WB");
                                break;
                            case 8:
                                NAME_APPEND("Am486DX4-WT");
                                break;
                            case 9:
                                NAME_APPEND("Am486DX4-WB");
                                break;
                            case 14:
                                NAME_APPEND("Am5x86-WT");
                                break;
                            case 15:
                                NAME_APPEND("Am5x86-WB");
                                break;
                            default:
                                goto unknown;
                        }
                        break;
                    default:
                        goto unknown;
                }
                break;
            unknown:
            default:
                strlcpy(this_cpu->name, "unrecognized", sizeof(this_cpu->name));
                break;
        }
    }

    if (max_function >= 0x80000006)
    {
        cpuid_read(0x80000006, &cpuid_regs);
        this_cpu->cacheline_size = cpuid_regs.ecx & 0xff;
        this_cpu->cache_size = (cpuid_regs.ecx >> 16) * KiB;
    }

    if (max_function >= 0x80000007)
    {
        cpuid_read(0x80000007, &cpuid_regs);
        cpu_features_save(CPUID_80000007, cpuid_regs.edx);
    }

#ifdef __i386__
    this_cpu->phys_bits = 32;
    this_cpu->virt_bits = 32;
#endif
}

UNMAP_AFTER_INIT int cpu_detect(bool bsp)
{
    uint32_t max_function, vendor;
    cpuid_regs_t cpuid_regs = {};

    cpu_info_t* this_cpu = THIS_CPU_GET(cpu_info);

    if (unlikely(!cpuid_available))
    {
        log_notice("CPUID not available");
        this_cpu->family = family;
        strcpy(this_cpu->vendor, "unknown");
        strcpy(this_cpu->producer, "unknown");
        strcpy(this_cpu->name, family == 4 ? "486" : "386");

        goto print;
    }

    cpuid_read(0, &cpuid_regs);

    // Vendor is found in EBX, EDX, ECX in extact order
    memcpy(this_cpu->vendor, &cpuid_regs.ebx, 4);
    memcpy(&this_cpu->vendor[4], &cpuid_regs.edx, 4);
    memcpy(&this_cpu->vendor[8], &cpuid_regs.ecx, 4);
    this_cpu->vendor[12] = 0;

    max_function = cpuid_regs.eax;
    vendor = cpuid_regs.ebx;

    this_cpu->max_function = max_function;

    if (max_function >= 1)
    {
        cpuid_read(1, &cpuid_regs);
        this_cpu->stepping = (cpuid_regs.eax) & 0xf;
        this_cpu->model = cpu_model(cpuid_regs.eax);
        this_cpu->family = cpu_family(cpuid_regs.eax);
        this_cpu->type = cpu_type(cpuid_regs.eax);
        this_cpu->lapic_id = cpuid_regs.ebx >> 24;

        // The AMD-K5 processor (model 0) reserves bit 13 and implements feature bit 9 to
        // indicate support for Global Paging Extensions instead of support for APIC
        if (this_cpu->vendor_id == AMD && this_cpu->family == 5 && this_cpu->model == 0)
        {
            int pge = (cpuid_regs.edx >> 9) & 1;
            cpuid_regs.edx &= ~((1 << 13) | (1 << 9));
            cpuid_regs.edx |= pge << 13;
        }

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

    this_cpu->vendor_id = vendor;

    switch (this_cpu->vendor_id)
    {
        case INTEL:
            snprintf(this_cpu->producer, sizeof(this_cpu->producer), "Intel");
            break;
        case AMD:
            snprintf(this_cpu->producer, sizeof(this_cpu->producer), "AMD");
            break;
        case CYRIX:
            snprintf(this_cpu->producer, sizeof(this_cpu->producer), "Cyrix");
            break;
        default:
            snprintf(this_cpu->producer, sizeof(this_cpu->producer), "unknown");
            break;
    }

    extended_functions_read(this_cpu);

    if ((this_cpu->family == 0xf && this_cpu->model >= 0x03) ||
        (this_cpu->family == 0x6 && this_cpu->model >= 0x0e))
    {
        cpu_feature_set(X86_FEATURE_INVTSC);
    }

print:
    if (!bsp)
    {
        return 0;
    }

    log_notice("producer: %s (%s), name: %s",
        this_cpu->producer,
        this_cpu->vendor,
        this_cpu->name);

    log_notice("family: %#x, model: %#x, type: %#x, stepping: %#x",
        this_cpu->family,
        this_cpu->model,
        this_cpu->type,
        this_cpu->stepping);

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

    log_info("max functions: %#x/%#x", this_cpu->max_function, this_cpu->max_extended_function);

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
