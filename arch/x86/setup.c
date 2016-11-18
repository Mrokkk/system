#include <kernel/process.h>
#include <kernel/mm.h>
#include <kernel/irq.h>
#include <kernel/time.h>
#include <kernel/module.h>

#include <arch/io.h>
#include <arch/descriptor.h>
#include <arch/cpuid.h>

void descriptor_init();

static inline void nmi_enable(void) {
    outb(0x70, inb(0x70) & 0x7f);
}

/* Not used */
#if 0
static inline void nmi_disable(void) {
    outb(0x70, inb(0x70) | 0x80);
}
#endif

void delay(unsigned int msec) {

    unsigned int current = jiffies;
    int i;

    if (msec < 50) { /* TODO: It may not be accurate... */
        for (i=0; i<950; i++)
            udelay(msec);
        return;
    }

    msec = msec / 10;

    while (jiffies < (current + msec));
}

unsigned int ram_get() {

    int i, index = 0;

    for (i=1; i<10; i++)
        if (mmap[i].type == MMAP_TYPE_AVL)
            index = i;

    return (mmap[index].base + mmap[index].size);

}

int arch_info_get(char *buffer) {

    int len = 0;

    if (buffer)
        len = sprintf(buffer, "CPU Producer: %s (%s)\nCPU Name: %s\n",
                cpu_info.producer, cpu_info.vendor, cpu_info.name);

    return len;

}

__noreturn void reboot_by_crash() {

    gdt_load(0);

    while (1);

}

void prepare_to_shutdown() {

    cli();
    modules_shutdown();

}

__noreturn void reboot() {

    unsigned char dummy = 0x02;

    prepare_to_shutdown();

    while (dummy & 0x02)
        dummy = inb(0x64);

    outb(0xfe, 0x64);
    halt();

    while (1);

}

static void cpu_intel() {

    struct cpuid_regs cpuid_regs;

    cpuid_read(1, &cpuid_regs);
    cpu_info.features = cpuid_regs.edx;

    /* Check how many extended functions we have */
    cpuid_read(0x80000000, &cpuid_regs);
    if (cpuid_regs.eax < 0x80000004)
        return;

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

    sprintf(cpu_info.producer, "Intel");

    cpuid_read(2, &cpuid_regs); /* TODO: Other functions checking */

}

int cpu_info_get() {

    struct cpuid_regs cpuid_regs;

    /* Call CPUID function #0 */
    cpuid_read(0, &cpuid_regs);

    /* Vendor is found in EBX, EDX, ECX in extact order */
    memcpy(cpu_info.vendor, &cpuid_regs.ebx, 5);
    memcpy(&cpu_info.vendor[4], &cpuid_regs.edx, 5);
    memcpy(&cpu_info.vendor[8], &cpuid_regs.ecx, 5);
    cpu_info.vendor[12] = 0;

    switch (cpuid_regs.ebx) {
        case 0x756e6547:     /* Intel CPU */
            cpu_intel();
            break;
        case 0x68747541:     /* AMD CPU */
            break;
        default:            /* Unknown CPU */
            break;
    }

    return 0;
}

void arch_setup() {
    cpu_info_get();

    nmi_enable();

}

