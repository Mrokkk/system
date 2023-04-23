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
} __packed;

#define CPUID_1_EDX_INDEX   0
#define CPUID_1_EDX_OFFSET  0
#define CPUID_1_EDX_MASK    (~0UL)
#define INTEL_FPU       (CPUID_1_EDX_INDEX * 32 + 0)
#define INTEL_VME       (CPUID_1_EDX_INDEX * 32 + 1)
#define INTEL_DE        (CPUID_1_EDX_INDEX * 32 + 2)
#define INTEL_PSE       (CPUID_1_EDX_INDEX * 32 + 3)
#define INTEL_TSC       (CPUID_1_EDX_INDEX * 32 + 4)
#define INTEL_MSR       (CPUID_1_EDX_INDEX * 32 + 5)
#define INTEL_PAE       (CPUID_1_EDX_INDEX * 32 + 6)
#define INTEL_MCE       (CPUID_1_EDX_INDEX * 32 + 7)
#define INTEL_CX8       (CPUID_1_EDX_INDEX * 32 + 8)
#define INTEL_APIC      (CPUID_1_EDX_INDEX * 32 + 9)
#define INTEL_RESERVED1 (CPUID_1_EDX_INDEX * 32 + 10)
#define INTEL_SEP       (CPUID_1_EDX_INDEX * 32 + 11)
#define INTEL_MTRR      (CPUID_1_EDX_INDEX * 32 + 12)
#define INTEL_PGE       (CPUID_1_EDX_INDEX * 32 + 13)
#define INTEL_MCA       (CPUID_1_EDX_INDEX * 32 + 14)
#define INTEL_CMOV      (CPUID_1_EDX_INDEX * 32 + 15)
#define INTEL_PAT       (CPUID_1_EDX_INDEX * 32 + 16)
#define INTEL_PSE_36    (CPUID_1_EDX_INDEX * 32 + 17)
#define INTEL_PSN       (CPUID_1_EDX_INDEX * 32 + 18)
#define INTEL_CLFSH     (CPUID_1_EDX_INDEX * 32 + 19)
#define INTEL_RESERVED2 (CPUID_1_EDX_INDEX * 32 + 20)
#define INTEL_DS        (CPUID_1_EDX_INDEX * 32 + 21)
#define INTEL_ACPI      (CPUID_1_EDX_INDEX * 32 + 22)
#define INTEL_MMX       (CPUID_1_EDX_INDEX * 32 + 23)
#define INTEL_FXSR      (CPUID_1_EDX_INDEX * 32 + 24)
#define INTEL_SSE       (CPUID_1_EDX_INDEX * 32 + 25)
#define INTEL_SSE2      (CPUID_1_EDX_INDEX * 32 + 26)
#define INTEL_SS        (CPUID_1_EDX_INDEX * 32 + 27)
#define INTEL_HTT       (CPUID_1_EDX_INDEX * 32 + 28)
#define INTEL_TM        (CPUID_1_EDX_INDEX * 32 + 29)
#define INTEL_IA64      (CPUID_1_EDX_INDEX * 32 + 30)
#define INTEL_PBE       (CPUID_1_EDX_INDEX * 32 + 31)

#define CPUID_1_ECX_INDEX   1
#define CPUID_1_ECX_OFFSET  0
#define CPUID_1_ECX_MASK    (1 << 3 | 1 << 0)
#define INTEL_SSE3      (CPUID_1_ECX_INDEX * 32 + 0)

#define CPUID_80000001_INDEX    1
#define CPUID_80000001_OFFSET   4
#define CPUID_80000001_MASK     (1 << 11 | 1 << 27 | 1 << 8)
#define INTEL_PREFETCHW (CPUID_80000001_INDEX * 32 + CPUID_80000001_OFFSET + 8)
#define INTEL_SYSCALL   (CPUID_80000001_INDEX * 32 + CPUID_80000001_OFFSET + 11)
#define INTEL_RDTSCP    (CPUID_80000001_INDEX * 32 + CPUID_80000001_OFFSET + 27)

#define CPUID_80000007_INDEX    1
#define CPUID_80000007_OFFSET   0
#define CPUID_80000007_MASK     (1 << 8)
#define INTEL_INVTSC    (CPUID_80000007_INDEX * 32 + CPUID_80000007_OFFSET + 8)

#define NR_FEATURES 2

#define cpu_features_save(name, val) \
    cpu_info.features[name##_INDEX] |= ((val) & name##_MASK) << name##_OFFSET

#define cpu_has(feature) \
    bitset_test(cpu_info.features, feature)

static inline void cpuid_read(uint32_t function, struct cpuid_regs* regs)
{
    asm volatile(
        "cpuid;"
        : "=a" (regs->eax), "=b" (regs->ebx), "=c" (regs->ecx), "=d" (regs->edx)
        : "a" (function)
        : "cc"
    );
}

int cpu_info_get(void);
