#pragma once

#include <stdint.h>
#include <kernel/bitset.h>
#include <kernel/compiler.h>

struct cpuid_regs
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
};

typedef enum
{
    DATA = 1,
    INSTRUCTION = 2,
    UNIFIED = 3,
} cache_type_t;

struct cpuid_cache
{
    uint8_t layer;
    cache_type_t type;
    uint32_t ways;
    uint32_t line_size;
    uint32_t partitions;
    uint32_t sets;
};

typedef enum
{
    INTEL   = 0x756e6547,
    AMD     = 0x68747541,
} vendor_t;

typedef struct cpuid_regs cpuid_regs_t;

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

#define CPUID_1_EDX_INDEX       0
#define CPUID_1_EDX_OFFSET      0
#define CPUID_1_EDX_MASK        (~0UL)
#define X86_FEATURE_FPU         (CPUID_1_EDX_INDEX * 32 + 0)
#define X86_FEATURE_VME         (CPUID_1_EDX_INDEX * 32 + 1)
#define X86_FEATURE_DE          (CPUID_1_EDX_INDEX * 32 + 2)
#define X86_FEATURE_PSE         (CPUID_1_EDX_INDEX * 32 + 3)
#define X86_FEATURE_TSC         (CPUID_1_EDX_INDEX * 32 + 4)
#define X86_FEATURE_MSR         (CPUID_1_EDX_INDEX * 32 + 5)
#define X86_FEATURE_PAE         (CPUID_1_EDX_INDEX * 32 + 6)
#define X86_FEATURE_MCE         (CPUID_1_EDX_INDEX * 32 + 7)
#define X86_FEATURE_CX8         (CPUID_1_EDX_INDEX * 32 + 8)
#define X86_FEATURE_APIC        (CPUID_1_EDX_INDEX * 32 + 9)
#define X86_FEATURE_RESERVED1   (CPUID_1_EDX_INDEX * 32 + 10)
#define X86_FEATURE_SEP         (CPUID_1_EDX_INDEX * 32 + 11)
#define X86_FEATURE_MTRR        (CPUID_1_EDX_INDEX * 32 + 12)
#define X86_FEATURE_PGE         (CPUID_1_EDX_INDEX * 32 + 13)
#define X86_FEATURE_MCA         (CPUID_1_EDX_INDEX * 32 + 14)
#define X86_FEATURE_CMOV        (CPUID_1_EDX_INDEX * 32 + 15)
#define X86_FEATURE_PAT         (CPUID_1_EDX_INDEX * 32 + 16)
#define X86_FEATURE_PSE_36      (CPUID_1_EDX_INDEX * 32 + 17)
#define X86_FEATURE_PSN         (CPUID_1_EDX_INDEX * 32 + 18)
#define X86_FEATURE_CLFSH       (CPUID_1_EDX_INDEX * 32 + 19)
#define X86_FEATURE_RESERVED2   (CPUID_1_EDX_INDEX * 32 + 20)
#define X86_FEATURE_DS          (CPUID_1_EDX_INDEX * 32 + 21)
#define X86_FEATURE_ACPI        (CPUID_1_EDX_INDEX * 32 + 22)
#define X86_FEATURE_MMX         (CPUID_1_EDX_INDEX * 32 + 23)
#define X86_FEATURE_FXSR        (CPUID_1_EDX_INDEX * 32 + 24)
#define X86_FEATURE_SSE         (CPUID_1_EDX_INDEX * 32 + 25)
#define X86_FEATURE_SSE2        (CPUID_1_EDX_INDEX * 32 + 26)
#define X86_FEATURE_SS          (CPUID_1_EDX_INDEX * 32 + 27)
#define X86_FEATURE_HTT         (CPUID_1_EDX_INDEX * 32 + 28)
#define X86_FEATURE_TM          (CPUID_1_EDX_INDEX * 32 + 29)
#define X86_FEATURE_IA64        (CPUID_1_EDX_INDEX * 32 + 30)
#define X86_FEATURE_PBE         (CPUID_1_EDX_INDEX * 32 + 31)

#define CPUID_1_ECX_INDEX       1
#define CPUID_1_ECX_OFFSET      0
#define CPUID_1_ECX_MASK        (1 << 3 | 1 << 0)
#define X86_FEATURE_SSE3        (CPUID_1_ECX_INDEX * 32 + 0)

#define CPUID_80000001_INDEX    1
#define CPUID_80000001_OFFSET   4
#define CPUID_80000001_MASK     (1 << 11 | 1 << 27 | 1 << 8)
#define X86_FEATURE_PREFETCHW   (CPUID_80000001_INDEX * 32 + CPUID_80000001_OFFSET + 8)
#define X86_FEATURE_SYSCALL     (CPUID_80000001_INDEX * 32 + CPUID_80000001_OFFSET + 11)
#define X86_FEATURE_RDTSCP      (CPUID_80000001_INDEX * 32 + CPUID_80000001_OFFSET + 27)

#define CPUID_80000007_INDEX    1
#define CPUID_80000007_OFFSET   0
#define CPUID_80000007_MASK     (1 << 8)
#define X86_FEATURE_INVTSC      (CPUID_80000007_INDEX * 32 + CPUID_80000007_OFFSET + 8)

#define NR_FEATURES 2

#define cpu_features_save(name, val) \
    cpu_info.features[name##_INDEX] |= ((val) & name##_MASK) << name##_OFFSET

#define cpu_feature_set(name) \
    bitset_set(cpu_info.features, name)

#define cpu_has(feature) \
    bitset_test(cpu_info.features, feature)

#define cpu_family(eax) \
    ({ \
        uint32_t model = (eax >> 8) & 0xf; \
        if (model == 0xf) \
        { \
            model += (eax >> 20) & 0xff; \
        } \
        model; \
    })

#define cpu_model(eax) \
    ({ \
        uint32_t model, family; \
        model = (eax >> 4) & 0xf; \
        family = cpu_family(eax); \
        if (family >= 0x6) \
        { \
            model += ((eax >> 16) & 0xf) << 4; \
        } \
        model; \
    })

static inline void cpuid_read(uint32_t function, cpuid_regs_t* regs)
{
    asm volatile(
        "cpuid;"
        : "=a" (regs->eax), "=b" (regs->ebx), "=c" (regs->ecx), "=d" (regs->edx)
        : "a" (function), "c" (regs->ecx)
        : "cc"
    );
}

int cpu_detect(void);
