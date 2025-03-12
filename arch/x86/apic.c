#define log_fmt(fmt) "apic: " fmt
#include <arch/io.h>
#include <arch/irq.h>
#include <arch/smp.h>
#include <arch/apic.h>
#include <arch/hpet.h>
#include <arch/i8253.h>
#include <arch/i8259.h>
#include <arch/descriptor.h>

#include <kernel/cpu.h>
#include <kernel/irq.h>
#include <kernel/time.h>
#include <kernel/kernel.h>
#include <kernel/page_mmio.h>

#define DEBUG_MADT 1
#define DEBUG_APIC 0
#define DEBUG_IOAPIC 1
#define HPET_CALIBRATION_LOOPS 16

#define apic_debug(...) ({ if (DEBUG_APIC) log_info(__VA_ARGS__); })
#define ioapic_debug(...) ({ if (DEBUG_IOAPIC) log_info(__VA_ARGS__); })

extern void timer_handler(void);
static int apic_timer_irq_enable(void);
static int apic_disable(void);
static uint32_t apic_timer_calibrate_by_i8253(void);
static uint32_t apic_timer_calibrate_by_hpet(void);
static int ioapic_setup(void);
static void ioapic_initialize(void);
static int ioapic_irq_enable(uint32_t irq, int flags);
void apic_eoi(uint32_t);

READONLY apic_t* apic;
READONLY ioapic_t* ioapic;

READONLY static struct
{
    uint8_t irq;
    uint8_t flags;
} overrides[16] = {
    {0xff, 0}, {0xff, 0}, {0xff, 0}, {0xff, 0}, {0xff, 0}, {0xff, 0}, {0xff, 0}, {0xff, 0},
    {0xff, 0}, {0xff, 0}, {0xff, 0}, {0xff, 0}, {0xff, 0}, {0xff, 0}, {0xff, 0}, {0xff, 0},
};

READONLY static uint32_t init_cnt;

static clock_source_t apic_timer_clock = {
    .name = "apic_timer",
    .enable_systick = &apic_timer_irq_enable,
};

static irq_chip_t ioapic_chip = {
    .name = "ioapic",
    .initialize = &ioapic_setup,
    .irq_enable = &ioapic_irq_enable,
    .irq_disable = NULL,
    .disable = &apic_disable,
    .eoi = &apic_eoi,
    .vector_offset = IOAPIC_IRQ_VECTOR_OFFSET,
};

#define MADT_LOG(type, ...) \
    if (DEBUG_MADT) \
    { \
        type(__VA_ARGS__); \
    }

static void madt_scan(uint8_t* lapic_ids, size_t* num_cores)
{
    uint32_t offset;
    madt_entry_t* entry;
    madt_t* madt = acpi_find("APIC");

    static_assert(offsetof(madt_entry_t, ioapic.address) == 4);
    static_assert(offsetof(madt_entry_t, lapic_nmi.lint) == 5);

    if (!madt)
    {
        return;
    }

    entry = ptr(addr(madt) + sizeof(madt_t));

    for (; addr(entry) < addr(madt) + madt->header.len; entry = ptr(addr(entry) + entry->len))
    {
        MADT_LOG(log_info, "type: %u ", entry->type);
        switch (entry->type)
        {
            case MADT_TYPE_LAPIC:
                MADT_LOG(log_continue, "(Local APIC), cpu id: %#x, id = %#x, flags = %#x",
                    entry->lapic.cpu_id, entry->lapic.apic_id, entry->lapic.flags);
                if (entry->lapic.flags)
                {
                    lapic_ids[(*num_cores)++] = entry->lapic.apic_id;
                }
                break;

            case MADT_TYPE_IOAPIC:
                MADT_LOG(log_continue, "(IOAPIC), id = %#x, address = %#x, gsi_base = %#x",
                    entry->ioapic.id, entry->ioapic.address, entry->ioapic.gsi);

                offset = entry->ioapic.address - page_beginning(entry->ioapic.address);
                ioapic = mmio_map_uc(page_beginning(entry->ioapic.address), PAGE_SIZE, "ioapic");
                ioapic = ptr(addr(ioapic) + offset);
                break;

            case MADT_TYPE_IOAPIC_OVERRIDE:
                MADT_LOG(log_continue, "(IOAPIC Interrupt Source Override), bus = %#x, irq = %u, gsi = %#x, flags = %#x",
                    entry->ioapic_override.bus,
                    entry->ioapic_override.irq,
                    entry->ioapic_override.gsi,
                    entry->ioapic_override.flags);

                overrides[entry->ioapic_override.irq].irq = entry->ioapic_override.gsi;
                overrides[entry->ioapic_override.irq].flags = entry->ioapic_override.flags;
                break;

            case MADT_TYPE_LAPIC_NMI:
                MADT_LOG(log_continue, "(Local APIC NMI), cpu id: %#x, LINT%u, flags: %#x",
                    entry->lapic_nmi.cpu_id,
                    entry->lapic_nmi.lint,
                    entry->lapic_nmi.flags);
                break;

            case MADT_TYPE_LAPIC_ADDR_OVERRIDE:
                MADT_LOG(log_continue, "(Local APIC Address Override), address: %llx",
                    entry->lapic_addr_override.address);
                break;

            case MADT_TYPE_X2LAPIC:
                MADT_LOG(log_continue, "(Local x2APIC), local x2APIC ID: %u, APIC ID: %u, flags: %#x",
                    entry->x2apic.x2apic_id,
                    entry->x2apic.apic_id,
                    entry->x2apic.flags);
                break;
        }
    }
}

void apic_error(void)
{
    log_notice("error IRQ: ESR: %#x", apic->esr);
    apic_eoi(0);
}

void apic_spurious(void)
{
    log_notice("spurious IRQ");
    apic_eoi(0);
}

void apic_ipi_send(uint8_t lapic_id, uint32_t value)
{
    apic->esr = 0;
    apic->icr_hi = lapic_id << 24;
    apic->icr_low = value;

    while (apic->icr_low & APIC_ICR_SEND_PENDING)
    {
        cpu_relax();
    }
}

extern void empty_isr(void);
extern void apic_error_isr(void);
extern void apic_spurious_isr(void);

UNMAP_AFTER_INIT void apic_initialize(void)
{
    uint32_t eax, edx, apic_base;
    uint8_t lapic_ids[CPU_COUNT];
    size_t num_cores = 0;

    static_assert(offsetof(apic_t, eoi) == 0xb0);
    static_assert(offsetof(apic_t, siv) == 0xf0);
    static_assert(offsetof(apic_t, esr) == 0x280);
    static_assert(offsetof(apic_t, icr_low) == 0x300);
    static_assert(offsetof(apic_t, icr_hi) == 0x310);
    static_assert(offsetof(apic_t, lvt_error) == 0x370);
    static_assert(offsetof(apic_t, lvt_lint0) == 0x350);
    static_assert(offsetof(apic_t, lvt_lint1) == 0x360);
    static_assert(offsetof(apic_t, timer_init_cnt) == 0x380);
    static_assert(offsetof(apic_t, timer_current_cnt) == 0x390);
    static_assert(offsetof(apic_t, timer_div) == 0x3e0);

    if (!cpu_has(X86_FEATURE_MSR))
    {
        log_notice("cpu does not support MSR");
        return;
    }

    // FIXME: IBM T42's Pentium-M has an APIC and it should be possible to enable it even though APIC bit is 0
    // (this simply means that BIOS didn't enable it), however I don't know how to make it work - after force
    // enabling APIC, no interrupts are received by CPU. Another interesting thing, ICH4-M chipset has an IOAPIC,
    // but no MADT is reported
    if (!cpu_has(X86_FEATURE_APIC))
    {
        log_notice("cpu does not have APIC");
        return;
    }

    rdmsr(IA32_APIC_BASE_MSR, eax, edx);

    apic_base = eax & APIC_BASE_MASK;
    apic = mmio_map_uc(apic_base, PAGE_SIZE, "apic");

    eax |= IA32_APIC_BASE_MSR_ENABLE;

    wrmsr(IA32_APIC_BASE_MSR, eax, edx);

    apic->esr = 0;

    idt_set(APIC_SMP_NOTIF_VECTOR, addr(&empty_isr));
    idt_set(APIC_ERROR_IRQ_VECTOR, addr(&apic_error_isr));
    idt_set(APIC_SPURIOUS_IRQ_VECTOR, addr(&apic_spurious_isr));

    apic->lvt_timer = APIC_LVT_INT_MASKED;
    apic->lvt_error = APIC_ERROR_IRQ_VECTOR;
    apic->siv = APIC_SPURIOUS_IRQ_VECTOR | APIC_SIV_ENABLE;

    log_notice("base: %#x; id: %#x; bsp: %B; version: %#x",
        apic_base,
        apic->id,
        !!(eax & IA32_APIC_BASE_MSR_BSP),
        apic->version);

    rdmsr(IA32_APIC_BASE_MSR, eax, edx);

    switch (apic->version)
    {
        case 0x00 ... 0x0f:
            log_continue(" (82489DX discrete APIC)");
            break;
        case 0x10 ... 0x15:
            log_continue(" (Integrated APIC)");
            break;
        default:
            log_continue(" (Unknown APIC)");
    }

    madt_scan(lapic_ids, &num_cores);

    ioapic_initialize();
    smp_cpus_boot(lapic_ids, num_cores);
}

UNMAP_AFTER_INIT void apic_ap_initialize(void)
{
    apic->siv = APIC_SPURIOUS_IRQ_VECTOR | APIC_SIV_ENABLE;
    apic->esr = 0;
    apic->lvt_error = APIC_ERROR_IRQ_VECTOR;
    apic->lvt_timer = APIC_LVT_INT_MASKED;

    apic_eoi(0);
}

UNMAP_AFTER_INIT void apic_timer_initialize(void)
{
    if (!apic)
    {
        return;
    }

    uint32_t i8253_init_cnt = apic_timer_calibrate_by_i8253();
    uint32_t hpet_init_cnt = apic_timer_calibrate_by_hpet();
    init_cnt = hpet_init_cnt ? hpet_init_cnt : i8253_init_cnt;

    clock_source_register_khz(&apic_timer_clock, apic_timer_clock.freq_khz * 1000);
}

void apic_eoi(uint32_t)
{
    apic->eoi = 0;
}

static int apic_timer_irq_enable(void)
{
    if (!init_cnt)
    {
        return -EINVAL;
    }

    apic->lvt_timer = 32 | APIC_LVT_TIMER_PERIODIC;
    apic->timer_div = APIC_TIMER_DIV_16;
    apic->timer_init_cnt = init_cnt;

    irq_register(0, &timer_handler, "apic_timer", IRQ_NAKED);

    return 0;
}

static int apic_disable(void)
{
    apic->siv &= ~APIC_SIV_ENABLE;
    return 0;
}

static uint32_t apic_timer_calibrate_by_i8253(void)
{
    uint32_t ticks, mhz, mhz_remainder;

    // Set divisor to 16
    apic->timer_div = APIC_TIMER_DIV_16;

    // Enable the gate (with speaker disabled)
    outb((inb(NMI_SC_PORT) & ~NMI_SC_SPKR_DAT_EN) | NMI_SC_TIM_CNT2_EN, NMI_SC_PORT);

    // Configure PIT channel 2 to mode 0, lo/hi, binary, 10ms
    i8253_configure(I8253_CHANNEL2, I8253_MODE_0 | I8253_ACCESS_LOHI | I8253_16BIN, FREQ_100HZ);

    // Write counter to ~0UL
    apic->timer_init_cnt = APIC_TIMER_MAXCNT;

    // Wait for gate to be set and measure timer counter
    while (!(inb(NMI_SC_PORT) & NMI_SC_TMR2_OUT_STS));
    ticks = APIC_TIMER_MAXCNT - apic->timer_current_cnt;

    apic_timer_clock.freq_khz = mhz = ticks * 16;
    do_div(apic_timer_clock.freq_khz, KHz / FREQ_100HZ);
    mhz_remainder = do_div(mhz, MHz / FREQ_100HZ);

    log_notice("detected timer: %u.%04u MHz [i8253 calibration]", mhz, mhz_remainder);

    return ticks;
}

static uint32_t apic_timer_calibrate_by_hpet(void)
{
    uint32_t ticks, hpet_max, mhz, mhz_remainder;

    if (!hpet_available())
    {
        return 0;
    }

    hpet_disable();

    // QEMU requires a longer calibration, otherwise freq will be +/- 2MHz; with this it's +/- 0.05MHz
    hpet_max = (hpet_freq_get() / FREQ_100HZ) * HPET_CALIBRATION_LOOPS + hpet_cnt_value();

    // Set divisor to 16
    apic->timer_div = APIC_TIMER_DIV_16;

    // Write counter to ~0UL
    hpet_enable();
    apic->timer_init_cnt = APIC_TIMER_MAXCNT;

    while (hpet_cnt_value() < hpet_max);
    ticks = APIC_TIMER_MAXCNT - apic->timer_current_cnt;

    // Disable HPET
    hpet_disable();

    apic_timer_clock.freq_khz = mhz = ticks;
    do_div(apic_timer_clock.freq_khz, KHz / FREQ_100HZ);
    mhz_remainder = do_div(mhz, MHz / FREQ_100HZ);

    log_notice("detected timer: %u.%04u MHz [hpet calibration]", mhz, mhz_remainder);

    return ticks / HPET_CALIBRATION_LOOPS;
}

#undef log_fmt
#define log_fmt(fmt) "ioapic: " fmt

static uint32_t ioapic_read(void* addr, uint8_t reg)
{
    volatile struct ioapic* ioapic = addr;
    writel(reg, &ioapic->index);
    return readl(&ioapic->data);
}

static void ioapic_write(void* addr, uint8_t reg, uint32_t data)
{
    volatile struct ioapic* ioapic = addr;
    writel(reg, &ioapic->index);
    writel(data, &ioapic->data);
}

#define DWORD(data, i) \
    ({ \
        typecheck_ptr(data); \
        ((uint32_t*)data)[i]; \
    })

static void ioapic_configure(uint8_t reg, ioapic_route_t* route)
{
    ioapic_write(ioapic, reg, DWORD(route, 0));
    ioapic_write(ioapic, reg + 1, DWORD(route, 1));
}

static int ioapic_irq_enable(uint32_t irq, int)
{
    bool active_low = false, level_triggered = false;
    uint8_t vector = irq + IOAPIC_IRQ_VECTOR_OFFSET;
    uint8_t ioapic_irq = irq;

    if (overrides[irq].irq != 0xff)
    {
        ioapic_irq = overrides[irq].irq;
        switch (overrides[irq].flags & 0x3)
        {
            case 0x0:
            case 0x1:
                active_low = false;
                break;
            case 0x2:
                log_error("BIOS bug: polarity is 0x2");
                return -EINVAL;
            case 0x3:
                active_low = true;
                break;
        }

        switch ((overrides[irq].flags >> 2) & 0x3)
        {
            case 0x0:
            case 0x1:
                level_triggered = false;
                break;
            case 0x2:
                log_error("BIOS bug: trigger mode is 0x2");
                return -EINVAL;
            case 0x3:
                level_triggered = true;
                break;
        }
    }

    ioapic_route_t route = {
        .interrupt_vector = vector,
        .type = 0x0,
        .destination_mode = 0,
        .polarity = active_low,
        .trigger_mode = level_triggered,
        .mask = 0,
        .destination = 0,
    };

    ioapic_debug("IRQ%u routed through pin %u; vector: %u; active %s %s triggered",
        irq, ioapic_irq, vector, active_low ? "low" : "high", level_triggered ? "level" : "edge");

    ioapic_configure(IOAPIC_REG_REDIRECT + ioapic_irq * 2, &route);

    // FIXME: HP ProBook 6470b: if no EOI is done when using PIT, it freezes whole IOAPIC
    {
        apic_eoi(irq);
    }

    return 0;
}

static void ioapic_initialize(void)
{
    uint32_t data, id;

    if (!ioapic)
    {
        log_notice("no IOAPIC");
        return;
    }

    id = (ioapic_read(ioapic, 0x0) >> 24) & 0xf;
    data = ioapic_read(ioapic, 0x1);
    ioapic_debug("id: %u, version: %#x, routes: %u", id, data & 0xff, (data >> 16) + 1);

    irq_chip_register(&ioapic_chip);
}

static int ioapic_setup(void)
{
    i8259_disable();
    return 0;
}
