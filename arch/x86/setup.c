#include <arch/io.h>
#include <arch/cpuid.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/descriptor.h>

#include <kernel/cpu.h>
#include <kernel/irq.h>
#include <kernel/page.h>
#include <kernel/time.h>
#include <kernel/memory.h>
#include <kernel/module.h>
#include <kernel/reboot.h>
#include <kernel/process.h>

volatile unsigned int jiffies;
struct cpu_info cpu_info;
static uint32_t mhz;
static uint64_t tsc_prev;
static ts_t timestamp;

static inline void nmi_enable(void)
{
    outb(0x70, inb(0x70) & 0x7f);
}

static inline void nmi_disable(void)
{
    outb(0x70, inb(0x70) | 0x80);
}

void timestamp_update()
{
    uint64_t tsc;
    uint32_t diff;

    if (!mhz)
    {
        return;
    }

    rdtscll(tsc);
    diff = (uint32_t)(tsc - tsc_prev);
    tsc_prev = tsc;

    // TODO: avoid div
    timestamp.useconds += diff / mhz;
    ts_align(&timestamp);
}

void timestamp_get(ts_t* ts)
{
    memcpy(ts, &timestamp, sizeof(*ts));
}

#define TSC_MEAS_LOOPS 8

void delay_calibrate(void)
{
    unsigned int tsc_meas_prec = TSC_MEAS_LOOPS;
    unsigned int ticks;
    flags_t flags;
    uint64_t tsc_start, tsc_end;

    if (!cpu_has(INTEL_TSC))
    {
        log_warning("tsc not available!");
        return;
    }

    log_info("detecting TSC...");

    irq_save(flags);
    sti();

    ticks = jiffies;
    while (tsc_meas_prec--)
    {
        while (ticks == jiffies);
        rdtscll(tsc_start);
        ticks = jiffies;
        while (ticks == jiffies);
        rdtscll(tsc_end);
        ticks = jiffies;
        mhz += tsc_end - tsc_start;
    }

    tsc_prev = tsc_end;

    irq_restore(flags);

    mhz /= TSC_MEAS_LOOPS * (UNIT_MHZ / HZ);

    log_info("detected %u MHz TSC", mhz);

    cpu_info.mhz = mhz;

    if (cpu_has(INTEL_RDTSCP))
    {
        log_info("aligning time using RDTSCP...");
        register uint32_t low, hi, dummy;
        asm volatile("rdtscp" : "=a" (low), "=c" (dummy), "=d" (hi));

        low /= mhz;
        timestamp.seconds = low / UNIT_MHZ;
        timestamp.useconds = low % UNIT_MHZ;

        timestamp.seconds += hi * 4294 / mhz;
        timestamp.useconds += 967296 / mhz;

        ts_align(&timestamp);
    }
}

void arch_reboot(int cmd)
{
    if (cmd == REBOOT_CMD_RESTART)
    {
        log_notice("performing reboot...");
        uint8_t dummy = 0x02;

        while (dummy & 0x02)
        {
            dummy = inb(0x64);
        }

        outb(0xfe, 0x64);

        for (;; halt());
    }
}

void arch_setup()
{
    ASSERT(cs_get() == KERNEL_CS);
    ASSERT(ds_get() == KERNEL_DS);
    ASSERT(gs_get() == KERNEL_DS);
    ASSERT(ss_get() == KERNEL_DS);

    idt_init();
    tss_init();
    cpu_info_get();
    nmi_enable();

    log_info("cpu: producer: %s (%s), name: %s",
        cpu_info.producer,
        cpu_info.vendor,
        cpu_info.name);

    for (uint32_t i = 0; i < 3; ++i)
    {
        log_info("L%u cache: %s", i + 1, cpu_info.cache[i].description);
    }

    cpu_features_string_get(cpu_info.features_string);

    log_info("features: %s", cpu_info.features_string);

    if (cpu_has(INTEL_SSE))
    {
        extern void sse_enable(void);
        sse_enable();
    }
}
