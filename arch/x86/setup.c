#include <arch/io.h>
#include <arch/apm.h>
#include <arch/dmi.h>
#include <arch/irq.h>
#include <arch/nmi.h>
#include <arch/pci.h>
#include <arch/rtc.h>
#include <arch/tsc.h>
#include <arch/vbe.h>
#include <arch/acpi.h>
#include <arch/apic.h>
#include <arch/bios.h>
#include <arch/hpet.h>
#include <arch/cpuid.h>
#include <arch/i8042.h>
#include <arch/i8253.h>
#include <arch/i8259.h>
#include <arch/bios32.h>
#include <arch/memory.h>
#include <arch/segment.h>
#include <arch/earlycon.h>
#include <arch/register.h>
#include <arch/descriptor.h>

#include <kernel/cpu.h>
#include <kernel/elf.h>
#include <kernel/irq.h>
#include <kernel/init.h>
#include <kernel/page.h>
#include <kernel/time.h>
#include <kernel/clock.h>
#include <kernel/memory.h>
#include <kernel/module.h>
#include <kernel/reboot.h>
#include <kernel/process.h>

struct cpu_info cpu_info;
static bool panic_mode;

static void reboot_by_8042(void)
{
    while (inb(0x64) & 0x02);

    io_delay(10);
    outb(0xfc, 0x64);
    io_delay(10);
}

static void shutdown_dummy(void)
{
    log_notice("you may power off the computer...");
    for (;; halt());
}

UNMAP_AFTER_INIT void arch_setup(void)
{
    ASSERT(cs_get() == KERNEL_CS);
    ASSERT(ds_get() == KERNEL_DS);
    ASSERT(gs_get() == KERNEL_DS);
    ASSERT(ss_get() == KERNEL_DS);

    param_call_if_set(KERNEL_PARAM("earlycon"), &earlycon_init);

    reboot_fn = &reboot_by_8042;
    shutdown_fn = &shutdown_dummy;

    idt_init();
    tss_init();
    nmi_enable();
    cpu_detect();
    memory_detect();
    bios32_init();
    rtc_print();

    if (cpu_has(X86_FEATURE_SSE))
    {
        asm volatile(
            "mov %cr0, %eax;"
            "and $0xfffb, %ax;"
            "or $0x2, %ax;"
            "mov %eax, %cr0;"
            "mov %cr4, %eax;"
            "or $(3 << 9), %ax;"
            "mov %eax, %cr4;"
            "movb $1, (%eax);");
    }
}

UNMAP_AFTER_INIT void arch_late_setup(void)
{
    dmi_read();
    pci_scan();

    apm_initialize();

    acpi_initialize();

    i8259_preinit();
    apic_initialize();

    i8042_initialize();

    i8253_initialize();
    hpet_initialize();
    apic_timer_initialize();
    tsc_initialize();

    irqs_initialize();

    rtc_initialize();
    clock_sources_setup();
    time_setup();

    vbe_initialize();

    // Make sure PIC is in proper state
    i8259_check();

    elf_register();

    clear_first_pde();
}

void panic_mode_enter(void)
{
    if (panic_mode)
    {
        return;
    }

    ensure_printk_will_print();

    // Identity map first 1MiB so that
    // earlycon and bios calls will work
    page_map_panic(0, 0x100000);

    earlycon_enable();
    panic_mode = true;
}

void panic_mode_die(void)
{
    printk(KERN_CRIT "Press ENTER to reboot...");

    while (i8042_receive() != 0x1c);

    machine_reboot();
}
