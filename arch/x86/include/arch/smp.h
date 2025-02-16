#pragma once

#define SMP_SIGNATURE 0xa1b2c3d4

#define SMP_CODE_AREA       0x8000
#define SMP_SIGNATURE_AREA  0x9000
#define SMP_CPU_SIZE        36
#define SMP_STACK_OFFSET    12
#define SMP_CPU_DATA_OFFSET 16
#define SMP_GDT_OFFSET      20
#define SMP_CR0_OFFSET      24
#define SMP_CR3_OFFSET      28
#define SMP_CR4_OFFSET      32

#define CPU_COUNT 16

#ifndef __ASSEMBLER__

#include <stddef.h>
#include <stdint.h>
#include <arch/io.h>
#include <arch/apic.h>

struct cpu_boot
{
    io32     cpu_signature;
    io32     lapic_id;
    io32     signature;
    void*    stack;
    void*    cpu_data;
    gdt_t*   gdt;
    uint32_t cr0;
    uint32_t cr3;
    uint32_t cr4;
};

typedef struct cpu_boot cpu_boot_t;

enum
{
    SMP_IDLE_START = 1,
};

void smp_cpus_boot(uint8_t* lapic_ids, size_t num);
void smp_notify(uint8_t notif);
void smp_wait_for(uint8_t notif);

#endif
