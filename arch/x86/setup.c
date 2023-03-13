#include <arch/io.h>
#include <arch/page.h>
#include <arch/cpuid.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/descriptor.h>

#include <kernel/cpu.h>
#include <kernel/irq.h>
#include <kernel/time.h>
#include <kernel/memory.h>
#include <kernel/module.h>
#include <kernel/reboot.h>
#include <kernel/process.h>

// This is the number of bits of precision for the loops_per_second.  Each
// bit takes on average 1.5/HZ seconds.  This (like the original) is a little
// better than 1%
#define LPS_PREC 8

volatile unsigned int jiffies;
struct cpu_info cpu_info;
uint32_t loops_per_sec = (1 << 12);

static inline void nmi_enable(void)
{
    outb(0x70, inb(0x70) & 0x7f);
}

static inline void nmi_disable(void)
{
    outb(0x70, inb(0x70) | 0x80);
}

void delay_calibrate(void)
{
    unsigned int ticks;
    int loopbit;
    int lps_precision = LPS_PREC;
    int flags;

    irq_save(flags);
    sti();

    log_debug("calibrating delay loop.. ");
    while (loops_per_sec <<= 1)
    {
        // wait for "start of" clock tick
        ticks = jiffies;
        while (ticks == jiffies);
        // Go
        ticks = jiffies;
        do_delay(loops_per_sec);
        ticks = jiffies - ticks;
        if (ticks)
        {
            break;
        }
    }

    // Do a binary approximation to get loops_per_second set to equal one clock
    // (up to lps_precision bits)
    loops_per_sec >>= 1;
    loopbit = loops_per_sec;

    while (lps_precision-- && (loopbit >>= 1) )
    {
        loops_per_sec |= loopbit;
        ticks = jiffies;
        while (ticks == jiffies);
        ticks = jiffies;
        do_delay(loops_per_sec);
        if (jiffies != ticks) // longer than 1 tick
        {
            loops_per_sec &= ~loopbit;
        }
    }

    // finally, adjust loops per second in terms of seconds instead of clocks
    loops_per_sec *= HZ;
    // Round the value and print it

    log_debug("ok - %lu.%02lu BogoMIPS",
        (loops_per_sec + 2500) / 500000,
        ((loops_per_sec + 2500) / 5000) % 100);

    cpu_info.bogomips = loops_per_sec;

    irq_restore(flags);
}

void delay(uint32_t msec)
{
    uint32_t current = jiffies;

    // FIXME: it's not accurate
    if (msec < 50)
    {
        for (int i = 0; i < 950; i++)
        {
            udelay(msec);
        }
        return;
    }

    msec = msec / 10;

    while (jiffies < (current + msec));
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
        log_info("L%u cache: %s", i, cpu_info.cache[i].description);
    }

    char buffer[150];
    cpu_features_string_get(buffer);

    log_info("features: %s", buffer);

    if (cpu_info.features & INTEL_SSE)
    {
        extern void sse_enable(void);
        sse_enable();
    }
}
