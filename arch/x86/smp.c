#define log_fmt(fmt) "smp: " fmt
#include <arch/smp.h>
#include <arch/idle.h>
#include <arch/percpu.h>
#include <arch/register.h>
#include <kernel/cpu.h>
#include <kernel/init.h>
#include <kernel/kernel.h>
#include <kernel/sections.h>
#include <kernel/spinlock.h>
#include <kernel/page_alloc.h>

extern void smp_entry(void);
extern void smp_entry_end(void);

READONLY static io32 booted = 1;
READONLY void* per_cpu_data[CPU_COUNT];
volatile uint8_t notification;

static_assert(sizeof(cpu_boot_t) == SMP_CPU_SIZE);
static_assert(offsetof(cpu_boot_t, stack) == SMP_STACK_OFFSET);
static_assert(offsetof(cpu_boot_t, cr0) == SMP_CR0_OFFSET);
static_assert(offsetof(cpu_boot_t, cr3) == SMP_CR3_OFFSET);
static_assert(offsetof(cpu_boot_t, cr4) == SMP_CR4_OFFSET);

void smp_notify(uint8_t notif)
{
    if (apic && booted > 1)
    {
        notification = notif;
        apic_ipi_send(0, APIC_SMP_NOTIF_VECTOR | APIC_ICR_DEST_ALL_SELF_EXCLUDE);
    }
}

void smp_wait_for(uint8_t notif)
{
    while (notification != notif)
    {
        halt();
    }
}

UNMAP_AFTER_INIT void bsp_per_cpu_setup(void)
{
    *(void**)_data_per_cpu_start = _data_per_cpu_start;
    descriptor_set_base(gdt_entries, KERNEL_PER_CPU_DATA, addr(_data_per_cpu_start));
    asm volatile("mov %0, %%fs" :: "r" (KERNEL_PER_CPU_DS) : "memory");
}

static gdt_t* cpu_gdt_setup(uint8_t lapic_id)
{
    gdt_t* this_gdt = CPU_GET(lapic_id, &gdt);
    gdt_entry_t* this_gdt_entries = CPU_GET(lapic_id, (gdt_entry_t*)gdt_entries);
    size_t gdt_entries_size = addr(gdt_entries_end) - addr(gdt_entries);

    memcpy(this_gdt_entries, gdt_entries, gdt_entries_size);

    descriptor_set_base(this_gdt_entries, KERNEL_PER_CPU_DATA, addr(per_cpu_data[lapic_id]));
    this_gdt->base = phys_addr(this_gdt_entries);
    this_gdt->limit = gdt.limit;

    return this_gdt;
}

static void cpu_boot(uint8_t lapic_id, void* stack, void* cpu_data, uint32_t cr0, uint32_t cr3, uint32_t cr4)
{
    log_notice("cpu%u: booting", lapic_id);

    cpu_boot_t* cpu = (cpu_boot_t*)SMP_SIGNATURE_AREA + lapic_id;

    per_cpu_data[lapic_id] = cpu_data;
    *(void**)cpu_data = cpu_data;

    cpu->stack    = stack;
    cpu->gdt      = cpu_gdt_setup(lapic_id);
    cpu->cpu_data = cpu_data;
    cpu->cr0      = cr0;
    cpu->cr3      = cr3;
    cpu->cr4      = cr4;

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

UNMAP_AFTER_INIT void smp_cpus_boot(uint8_t* lapic_ids, size_t count)
{
    if (count == 1)
    {
        return;
    }

    per_cpu_data[cpu_info.lapic_id] = _data_per_cpu_start;
    memcpy(ptr(SMP_CODE_AREA), &smp_entry, addr(&smp_entry_end) - addr(&smp_entry));
    memset(ptr(SMP_SIGNATURE_AREA), 0, PAGE_SIZE);

    uint32_t cr0 = cr0_get();
    uint32_t cr3 = cr3_get();
    uint32_t cr4 = cr4_get();

    log_notice("cpus: %u", count);

    size_t per_cpu_size = _data_per_cpu_end - _data_per_cpu_start;
    size_t pages_count = 1 + align(per_cpu_size, PAGE_SIZE) / PAGE_SIZE;

    for (size_t i = 0; i < count; ++i)
    {
        if (lapic_ids[i] == cpu_info.lapic_id)
        {
            continue;
        }

        page_t* pages = page_alloc(pages_count, PAGE_ALLOC_CONT);

        if (unlikely(!pages))
        {
            log_warning("cpu%u: cannot allocate memory per cpu data", lapic_ids[i]);
            return;
        }

        memset(page_virt_ptr(pages), 0, pages_count * PAGE_SIZE);

        cpu_boot(
            lapic_ids[i],
            page_virt_ptr(pages) + PAGE_SIZE,
            page_virt_ptr(pages) + PAGE_SIZE,
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

    apic_ap_initialize();
    cpu_detect(false);

    sti();

    smp_wait_for(SMP_IDLE_START);

    log_info("cpu%u: go idle", lapic_id);

    while (1)
    {
        halt();
    }
}
