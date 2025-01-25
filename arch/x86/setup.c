#define log_fmt(fmt) "x86-setup: " fmt
#include <arch/io.h>
#include <arch/apm.h>
#include <arch/asm.h>
#include <arch/dmi.h>
#include <arch/irq.h>
#include <arch/nmi.h>
#include <arch/pci.h>
#include <arch/rtc.h>
#include <arch/tsc.h>
#include <arch/acpi.h>
#include <arch/apic.h>
#include <arch/bios.h>
#include <arch/hpet.h>
#include <arch/mtrr.h>
#include <arch/cpuid.h>
#include <arch/i8042.h>
#include <arch/i8253.h>
#include <arch/i8259.h>
#include <arch/bios32.h>
#include <arch/memory.h>
#include <arch/segment.h>
#include <arch/earlycon.h>
#include <arch/register.h>
#include <arch/real_mode.h>
#include <arch/descriptor.h>
#include <arch/page_low_mem.h>

#include <kernel/cpu.h>
#include <kernel/elf.h>
#include <kernel/irq.h>
#include <kernel/vga.h>
#include <kernel/time.h>
#include <kernel/vesa.h>
#include <kernel/clock.h>
#include <kernel/memory.h>
#include <kernel/module.h>
#include <kernel/reboot.h>
#include <kernel/process.h>

#define VIRTIO_GPU_DISABLED 0

struct cpu_info cpu_info;
bool panic_mode;

int vgafb_initialize(void);

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

    real_mode_call_init();
    earlycon_init();

    reboot_fn = &reboot_by_8042;
    shutdown_fn = &shutdown_dummy;

    idt_init();
    tss_init();
    nmi_enable();
    cpu_detect();
    memory_detect();
    bios32_init();
    rtc_print();

#ifdef __i386__
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
#endif
}

int vsyscall_init(void);

UNMAP_AFTER_INIT void arch_late_setup(void)
{
    mtrr_initialize();

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

    int virtio_gpu_init();
    if (VIRTIO_GPU_DISABLED || virtio_gpu_init())
    {
        log_info("cannot initialize VirtIO GPU; trying VESA");
        if (vesafb_initialize())
        {
            log_info("cannot initialize VESA; using VGA");
            vgafb_initialize();
        }
    }

    // Make sure PIC is in proper state
    i8259_check();

    elf_register();

    vsyscall_init();
    real_mode_finalize();
}

void panic_mode_enter(void)
{
    if (panic_mode)
    {
        return;
    }

    cli();
    panic_mode = true;

    ensure_printk_will_print();

    // Identity map first 1MiB so that
    // earlycon and bios calls will work
    page_low_mem_map(NULL);

    earlycon_enable();
}

void panic_mode_die(void)
{
    log(KERN_CRIT, "Press ENTER to reboot...");

    while (i8042_receive() != 0x1c);

    machine_reboot();
}
