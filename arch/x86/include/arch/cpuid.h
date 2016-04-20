#ifndef __CPUID_H_
#define __CPUID_H_

#include <kernel/compiler.h>

struct cpuid_regs {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    unsigned int null;
} __attribute__ ((packed));

struct cpuid {
    char vendor[13];
    char brand[46];
    unsigned char brand_id;
    unsigned short model;
    unsigned short family;
    unsigned char type;
    unsigned short ext_model;
    unsigned short ext_family;
    unsigned short stepping;
    union {
        unsigned int intel_ext;
        unsigned int amd_ext;
    } dep;
};

#define INTEL_FPU       (1)
#define INTEL_VME       (1 << 1)
#define INTEL_DE        (1 << 2)
#define INTEL_PSE       (1 << 3)
#define INTEL_TSC       (1 << 4)
#define INTEL_MSR       (1 << 5)
#define INTEL_PAE       (1 << 6)
#define INTEL_MCE       (1 << 7)
#define INTEL_CX8       (1 << 8)
#define INTEL_APIC      (1 << 9)
#define INTEL_RESERVED1 (1 << 10)
#define INTEL_SEP       (1 << 11)
#define INTEL_MTRR      (1 << 12)
#define INTEL_PGE       (1 << 13)
#define INTEL_MCA       (1 << 14)
#define INTEL_CMOV      (1 << 15)
#define INTEL_PAT       (1 << 16)
#define INTEL_PSE_36    (1 << 17)
#define INTEL_PSN       (1 << 18)
#define INTEL_CLFSH     (1 << 19)
#define INTEL_RESERVED2 (1 << 20)
#define INTEL_DS        (1 << 21)
#define INTEL_ACPI      (1 << 22)
#define INTEL_MMX       (1 << 23)
#define INTEL_FXSR      (1 << 24)
#define INTEL_SSE       (1 << 25)
#define INTEL_SSE2      (1 << 26)
#define INTEL_SS        (1 << 27)
#define INTEL_HTT       (1 << 28)
#define INTEL_TM        (1 << 29)
#define INTEL_IA64      (1 << 30)
#define INTEL_PBE       (1 << 31)

#define INTEL_FPU_STRING    "FPU on chip"
#define INTEL_VME_STRING    "Virtual Mode Extension"
#define INTEL_DE_STRING     "Debugging Extension"
#define INTEL_PSE_STRING    "Page Size Extension"
#define INTEL_TSC_STRING    "Time Stamp Counter"
#define INTEL_MSR_STRING    "Model Specific Registers"
#define INTEL_PAE_STRING    "Physical Address Extension"
#define INTEL_MCE_STRING    "Machine Check Exception"
#define INTEL_CX8_STRING    "CMPXCHG8 Instruction Supported"
#define INTEL_APIC_STRING   "On-chip APIC"
#define INTEL_RESERVED1_STRING    "Reserved"
#define INTEL_SEP_STRING    "Fast System Call"
#define INTEL_MTRR_STRING   "Memory Type Range Registers"
#define INTEL_PGE_STRING    "Page Global Enable"
#define INTEL_MCA_STRING    "Machine Check Architecture"
#define INTEL_CMOV_STRING   "Conditional Move Instruction Supported"
#define INTEL_PAT_STRING    "Page Attribute Table"
#define INTEL_PSE_36_STRING "36-bit Page Size Extension"
#define INTEL_PSN_STRING    "Processor serial number is present and enabled"
#define INTEL_CLFSH_STRING  "CLFLUSH Instruction supported"
#define INTEL_RESERVED2_STRING "Reserved"
#define INTEL_DS_STRING     "Debug Store"
#define INTEL_ACPI_STRING   "ACPI supported"
#define INTEL_MMX_STRING    "Intel Architecture MMX technology supported"
#define INTEL_FXSR_STRING   "Fast floating point save and restore"
#define INTEL_SSE_STRING    "Streaming SIMD Extensions supported"
#define INTEL_SSE2_STRING   "Streaming SIMD Extensions 2"
#define INTEL_SS_STRING     "Self-Snoop"
#define INTEL_HTT_STRING    "Hyper-Threading Technology"
#define INTEL_TM_STRING     "Thermal Monitor supported"
#define INTEL_IA64_STRING   "IA64 processor emulating x86"
#define INTEL_PBE_STRING    "Pending Break Enable"

/*printd("CPU extensions:\n");
cpuid_print_ext(ext, INTEL_FPU);
cpuid_print_ext(ext, INTEL_VME);
cpuid_print_ext(ext, INTEL_DE);
cpuid_print_ext(ext, INTEL_PSE);
cpuid_print_ext(ext, INTEL_TSC);
cpuid_print_ext(ext, INTEL_MSR);
cpuid_print_ext(ext, INTEL_PAE);
cpuid_print_ext(ext, INTEL_MCE);
cpuid_print_ext(ext, INTEL_CX8);
cpuid_print_ext(ext, INTEL_APIC);
cpuid_print_ext(ext, INTEL_SEP);
cpuid_print_ext(ext, INTEL_MTRR);
cpuid_print_ext(ext, INTEL_PGE);
cpuid_print_ext(ext, INTEL_MCA);
cpuid_print_ext(ext, INTEL_CMOV);
cpuid_print_ext(ext, INTEL_PAT);
cpuid_print_ext(ext, INTEL_PSE_36);
cpuid_print_ext(ext, INTEL_PSN);
cpuid_print_ext(ext, INTEL_CLFSH);
cpuid_print_ext(ext, INTEL_DS);
cpuid_print_ext(ext, INTEL_ACPI);
cpuid_print_ext(ext, INTEL_MMX);
cpuid_print_ext(ext, INTEL_FXSR);
cpuid_print_ext(ext, INTEL_SSE);
cpuid_print_ext(ext, INTEL_SSE2);
cpuid_print_ext(ext, INTEL_SS);
cpuid_print_ext(ext, INTEL_HTT);
cpuid_print_ext(ext, INTEL_TM);
cpuid_print_ext(ext, INTEL_IA64);*/

#define cpuid_ext_avl(flags, ext) \
    ((flags) & (ext)) && 1

/*===========================================================================*
 *                                cpuid_read                                 *
 *===========================================================================*/
extern inline void cpuid_read(unsigned int function, struct cpuid_regs *regs) {

    asm volatile(
        "cpuid\n"
        : "=a" (regs->eax), "=b" (regs->ebx), "=c" (regs->ecx), "=d" (regs->edx)
        : "a" (function)
        : "cc"
    );

}

void cpuid_extensions_print(unsigned int ext);

#endif
