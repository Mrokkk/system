#include <kernel/process.h>
#include <kernel/mm.h>
#include <kernel/irq.h>
#include <kernel/time.h>
#include <kernel/module.h>

#include <arch/io.h>
#include <arch/descriptor.h>
#include <arch/cpuid.h>
#include <arch/segment.h>
#include <arch/multiboot.h>

static void tss_init();
static void idt_configure();
static void idt_set_gate(unsigned char num, unsigned long base, unsigned short selector, unsigned long flags);
int cpu_info_get();
static void cpu_intel();
void prepare_to_shutdown();

/* Exceptions */
#define __exception_noerrno(x) void exc_##x##_handler();
#define __exception_errno(x) void exc_##x##_handler();
#define __exception_debug(x) void exc_##x##_handler();
#include <arch/exception.h>

/* Interrupts */
#define __isr(x) void isr_##x();
#define __pic_isr(x) void isr_##x();
#define __timer_isr(x) void timer_handler();
#define __syscall_handler(x) void syscall_handler();
#include <arch/isr.h>

char *bootloader_name;
char cmdline_saved[64];

struct gdt_entry gdt_entries[] = {

    /* Null segment */
    descriptor_entry(0, 0, 0),

    /* Kernel code segment */
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_4KB | GDT_FLAGS_32BIT, 0x0, 0xffffffff),

    /* Kernel data segment */
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_4KB | GDT_FLAGS_32BIT, 0x0, 0xffffffff),

    /* User code segment */
    descriptor_entry(GDT_FLAGS_RING3 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_4KB | GDT_FLAGS_32BIT, 0x0, 0xffffffff),

    /* User data segment */
    descriptor_entry(GDT_FLAGS_RING3 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_4KB | GDT_FLAGS_32BIT, 0x0, 0xffff),

    /* TSS */
    descriptor_entry(GDT_FLAGS_TYPE_32TSS, 0, 0),

    /* APM code (32bit) */
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_1B | GDT_FLAGS_32BIT, 0, 0xffffffff),

    /* APM code (16bit) */
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_1B | GDT_FLAGS_16BIT, 0, 0xffffffff),

    /* APM data */
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_1B | GDT_FLAGS_32BIT, 0, 0xffffffff),

    /* LDT #1 */
    /* LDT #2 */
    /* ...... */

};

struct gdt gdt = {
        sizeof(struct gdt_entry) * 4096 - 1,
        (unsigned long)&gdt_entries
};

struct idt_entry idt_entries[256];

struct idt idt = {
        sizeof(struct idt_entry) * 256 - 1,
        (unsigned long)&idt_entries
};

struct gdt old_gdt = {
        0,
        0
};

struct gdt_entry *old_gdt_entries;

/*===========================================================================*
 *                                nmi_enable                                 *
 *===========================================================================*/
inline void nmi_enable(void) {
    outb(0x70, inb(0x70) & 0x7f);
}

/*===========================================================================*
 *                                nmi_disable                                *
 *===========================================================================*/
inline void nmi_disable(void) {
    outb(0x70, inb(0x70) | 0x80);
}

/*===========================================================================*
 *                                arch_setup                                 *
 *===========================================================================*/
void arch_setup() {

    /* Read CPU info */
    cpu_info_get();

    /* Store descriptors set by the bootloader */
    gdt_store(&old_gdt);
    old_gdt_entries = (struct gdt_entry *)old_gdt.base;

    gdt_load(&gdt);
    idt_configure();
    tss_init();
    nmi_enable();

}

/*===========================================================================*
 *                                 gdt_print                                 *
 *===========================================================================*/
void gdt_print(struct gdt_entry *entries, int size) {

    int i;

    for (i=0; i<size; i++) {
        printk("%u base 0x%x; ", i, descriptor_get_base(entries, i));
        printk("limit 0x%x; ", descriptor_get_limit(entries, i));
        printk("type 0x%x\n", descriptor_get_type(entries, i));
    }

}

/*===========================================================================*
 *                                   delay                                   *
 *===========================================================================*/
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

/*===========================================================================*
 *                                 tss_init                                  *
 *===========================================================================*/
static void tss_init() {

    unsigned int base = (unsigned int)&process_current->context;
    unsigned int limit = sizeof(struct tss) - 128; /* Set once */
    int i;

    descriptor_set_base(gdt_entries, FIRST_TSS_ENTRY, base);
    descriptor_set_limit(gdt_entries, FIRST_TSS_ENTRY, limit);

    /* Disable all ports */
    for (i=0; i<128; i++)
        process_current->context.io_bitmap[i] = 0xff;

    tss_load(tss_selector(0));

}

/*===========================================================================*
 *                                  ram_get                                  *
 *===========================================================================*/
unsigned int ram_get() {

    int i, index = 0;

    for (i=1; i<10; i++)
        if (mmap[i].type == MMAP_TYPE_AVL)
            index = i;

    return (mmap[index].base + mmap[index].size);

}

static void empty_isr() {};

/*===========================================================================*
 *                              idt_configure                                *
 *===========================================================================*/
static void idt_configure() {

    #undef __isr
    #undef __pic_isr
    #undef __timer_isr
    #undef __syscall_handler
    #define __isr(x) \
        idt_set_gate((x), (unsigned long)empty_isr, KERNEL_CS, \
                     GDT_FLAGS_TYPE_32TRAP_GATE);
    #define __pic_isr(x) \
        idt_set_gate((x), (unsigned long)isr_##x, KERNEL_CS, \
                     GDT_FLAGS_TYPE_32TRAP_GATE);
    #define __syscall_handler(x) \
        idt_set_gate((x), (unsigned long)syscall_handler, \
                 KERNEL_CS, GDT_FLAGS_TYPE_32TRAP_GATE | GDT_FLAGS_RING3);
    #define __timer_isr(x) \
        idt_set_gate((x), (unsigned long)timer_handler, \
                     KERNEL_CS, GDT_FLAGS_TYPE_32INT_GATE);

    #include <arch/isr.h>

    #undef __exception_noerrno
    #undef __exception_errno
    #undef __exception_debug
    #define __exception_noerrno(x) \
        idt_set_gate((__NR_##x), (unsigned long)exc_##x##_handler, \
                     KERNEL_CS, GDT_FLAGS_TYPE_32TRAP_GATE);
    #define __exception_errno __exception_noerrno
    #define __exception_debug __exception_noerrno
    #include <arch/exception.h>

    idt_load(&idt);

}

/*===========================================================================*
 *                               idt_set_gate                                *
 *===========================================================================*/
static void idt_set_gate(unsigned char num, unsigned long base,
                         unsigned short selector, unsigned long flags) {

    idt_entries[num].base_lo = (base) & 0xFFFF;
    idt_entries[num].base_hi = ((base) >> 16) & 0xFFFF;
    idt_entries[num].sel = (selector);
    idt_entries[num].always0 = 0;
    idt_entries[num].flags = flags | 0x80;

}

/*===========================================================================*
 *                              arch_info_print                              *
 *===========================================================================*/
int arch_info_get(char *buffer) {

    int len = 0;

    if (buffer)
        len = sprintf(buffer, "CPU Producer: %s (%s)\nCPU Name: %s\n",
                cpu_info.producer, cpu_info.vendor, cpu_info.name);

    return len;

}

/*===========================================================================*
 *                                   reboot                                  *
 *===========================================================================*/
__noreturn void reboot() {

    unsigned char dummy = 0x02;

    prepare_to_shutdown();

    while (dummy & 0x02)
        dummy = inb(0x64);

    outb(0xfe, 0x64);
    halt();

    while (1);

}

/*===========================================================================*
 *                              reboot_by_crash                              *
 *===========================================================================*/
__noreturn void reboot_by_crash() {

    gdt_load(0);

    while (1);

}

/*===========================================================================*
 *                             shutdown_by_bios                              *
 *===========================================================================*/
__noreturn void shutdown_by_bios() {
    while (1);
}

/*===========================================================================*
 *                           prepare_to_shutdown                             *
 *===========================================================================*/
void prepare_to_shutdown() {

    cli();
    modules_shutdown();

}

/*===========================================================================*
 *                                 shutdown                                  *
 *===========================================================================*/
__noreturn void shutdown() {

    prepare_to_shutdown();

    printk("Bye, bye!!!");

    while (1);

}

/*===========================================================================*
 *                               cpu_info_get                                *
 *===========================================================================*/
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

/*===========================================================================*
 *                                 cpu_intel                                 *
 *===========================================================================*/
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

/*===========================================================================*
 *                           multiboot_mmap_read                             *
 *===========================================================================*/
static inline void multiboot_mmap_read(struct multiboot_info *mb) {

    struct memory_map *mm;
    int i;

    for (mm = (struct memory_map *)mb->mmap_addr, i = 0;
         mm->base_addr_low + (mm->length_low - 1) != 0xffffffff;
         i++)
    {

        mm = (struct memory_map *)((unsigned int)mm + mm->size + 4);
        switch (mm->type) {
            case 1:
                mmap[i].type = MMAP_TYPE_AVL;
                break;
            case 2:
                mmap[i].type = MMAP_TYPE_NA;
                break;
            case 3:
                mmap[i].type = MMAP_TYPE_DEV;
                break;
            case 4:
                mmap[i].type = MMAP_TYPE_DEV;
                break;
            default:
                mmap[i].type = MMAP_TYPE_NDEF;
        }

        mmap[i].base = mm->base_addr_low;
        mmap[i].size = mm->length_low;
    }

    mmap[i].base = 0;

    ram = ram_get();

}

/*===========================================================================*
 *                        multiboot_boot_device_read                         *
 *===========================================================================*/
static inline void multiboot_boot_device_read(struct multiboot_info *mb) {

    (void)mb;

    //printk("boot device: %x\n", (unsigned int)mb->boot_device);

}

/*===========================================================================*
 *                          multiboot_modules_read                           *
 *===========================================================================*/
static inline void multiboot_modules_read(struct multiboot_info *mb) {

    int count = mb->mods_count, i;
    struct module *mod = (struct module *)mb->mods_addr;

    for (i=0; i<count; i++) {
        printk("%d: %s = 0x%x : 0x%x\n", i, (char *)mod->string+1,
                (unsigned int)mod->mod_start, (unsigned int)mod->mod_end);
        if (!strcmp((const char *)mod->string+1, "symbols")) {
            symbols_read((char *)mod->mod_start, mod->mod_end - mod->mod_start);
        }
        mod++;
    }

}

/*===========================================================================*
 *                              multiboot_read                               *
 *===========================================================================*/
int multiboot_read(struct multiboot_info *mb, unsigned int magic) {

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        printk("ERROR: Not Multiboot v1 compilant bootloader!\n");
        return 0;
    }

    if (mb->flags & MULTIBOOT_FLAGS_MMAP_BIT)
        multiboot_mmap_read(mb);
    if (mb->flags & MULTIBOOT_FLAGS_BOOTDEV_BIT)
        multiboot_boot_device_read(mb);
    if (mb->flags & MULTIBOOT_FLAGS_CMDLINE_BIT)
        strcpy(cmdline_saved, (char *)mb->cmdline);
    if (mb->flags & MULTIBOOT_FLAGS_BL_NAME_BIT)
        bootloader_name = (char *)mb->bootloader_name;
    if (mb->flags & MULTIBOOT_FLAGS_MODS_BIT) {
        multiboot_modules_read(mb);
    }

    return mb->cmdline;

}


