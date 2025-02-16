#define log_fmt(fmt) "smp: " fmt
#include <arch/smp.h>
#include <arch/register.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>
#include <kernel/page_alloc.h>

extern void smp_entry(void);
extern void smp_entry_end(void);

static io32 booted = 1;

static_assert(sizeof(cpu_boot_t) == SMP_CPU_SIZE);
static_assert(offsetof(cpu_boot_t, stack) == SMP_STACK_OFFSET);
static_assert(offsetof(cpu_boot_t, cr0) == SMP_CR0_OFFSET);
static_assert(offsetof(cpu_boot_t, cr3) == SMP_CR3_OFFSET);
static_assert(offsetof(cpu_boot_t, cr4) == SMP_CR4_OFFSET);

static void cpu_boot(uint8_t lapic_id, void* stack, uint32_t cr0, uint32_t cr3, uint32_t cr4)
{
    log_notice("cpu%u: booting", lapic_id);

    cpu_boot_t* cpu = (cpu_boot_t*)SMP_SIGNATURE_AREA + lapic_id;

    cpu->stack = stack;
    cpu->cr0   = cr0;
    cpu->cr3   = cr3;
    cpu->cr4   = cr4;

    // Assert INIT
    apic_ipi_send(lapic_id, APIC_ICR_DELIVERY_INIT | APIC_ICR_LEVEL_ASSERT | APIC_ICR_TRIGGER_LEVEL);

    // Deassert INIT
    apic_ipi_send(lapic_id, APIC_ICR_DELIVERY_INIT | APIC_ICR_TRIGGER_LEVEL);

    io_delay(200);

    // Send STARTUP twice
    for (int i = 0; i < 2; i++)
    {
        apic_ipi_send(lapic_id, APIC_ICR_DELIVERY_START_UP | (SMP_CODE_AREA / PAGE_SIZE));
        io_delay(200);
    }
}

static void cpu_shutdown(uint8_t lapic_id)
{
    // Assert INIT
    apic_ipi_send(lapic_id, APIC_ICR_DELIVERY_INIT | APIC_ICR_LEVEL_ASSERT | APIC_ICR_TRIGGER_LEVEL);

    // Deassert INIT
    apic_ipi_send(lapic_id, APIC_ICR_DELIVERY_INIT | APIC_ICR_TRIGGER_LEVEL);
    io_delay(200);

    // Assert INIT
    apic_ipi_send(lapic_id, APIC_ICR_DELIVERY_INIT | APIC_ICR_LEVEL_ASSERT | APIC_ICR_TRIGGER_LEVEL);
}

static void cpus_boot_timeout(uint8_t* lapic_ids, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        uint8_t lapic_id = lapic_ids[i];

        if (lapic_id == cpu_info.lapic_id)
        {
            continue;
        }

        cpu_boot_t* cpu = (cpu_boot_t*)SMP_SIGNATURE_AREA + lapic_id;
        if (cpu->signature != SMP_SIGNATURE)
        {
            log_warning("cpu%u: failed to boot", lapic_id);
            cpu_shutdown(lapic_id);
        }
    }
}

void cpus_boot(uint8_t* lapic_ids, size_t count)
{
    if (count == 1)
    {
        return;
    }

    memcpy(ptr(SMP_CODE_AREA), &smp_entry, addr(&smp_entry_end) - addr(&smp_entry));
    memset(ptr(SMP_SIGNATURE_AREA), 0, PAGE_SIZE);

    uint32_t cr0 = cr0_get();
    uint32_t cr3 = cr3_get();
    uint32_t cr4 = cr4_get();

    log_notice("cpus: %u", count);

    for (size_t i = 0; i < count; ++i)
    {
        if (lapic_ids[i] == cpu_info.lapic_id)
        {
            continue;
        }

        page_t* kernel_stack = page_alloc(1, 0);

        if (unlikely(!kernel_stack))
        {
            log_warning("cannot allocate memory for kernel stack for cpu%u", i);
            return;
        }

        cpu_boot(
            lapic_ids[i],
            page_virt_ptr(kernel_stack) + PAGE_SIZE,
            cr0,
            cr3,
            cr4);
    }

    int timeout = 10000;

    while (booted != count && --timeout)
    {
        io_delay(50);
    }

    if (unlikely(!timeout))
    {
        cpus_boot_timeout(lapic_ids, count);
        return;
    }

    log_info("all cpus booted sucessfully");
}

void smp_kmain(uint8_t lapic_id, uint32_t cpu_signature)
{
    cpu_boot_t* cpu = (cpu_boot_t*)SMP_SIGNATURE_AREA + lapic_id;

    cpu->lapic_id      = lapic_id;
    cpu->cpu_signature = cpu_signature;
    cpu->signature     = SMP_SIGNATURE;
    ++booted;

    log_notice("cpu%u: signature: %#x, lapic id: %#x",
        lapic_id,
        cpu->cpu_signature,
        cpu->lapic_id);

    while (1)
    {
        halt();
    }
}
