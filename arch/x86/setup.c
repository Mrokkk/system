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

// This is the number of bits of precision for the loops_per_second.  Each
// bit takes on average 1.5/HZ seconds.  This (like the original) is a little
// better than 1%
#define LPS_PREC 8

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
    extern void do_delay();
    unsigned int tsc_meas_prec = TSC_MEAS_LOOPS;
    unsigned int ticks;
    flags_t flags;
    uint64_t tsc_start, tsc_end;

    /*irq_save(flags);*/
    /*sti();*/

    /*static uint32_t loops_per_sec = (1 << 12);*/
    /*int loopbit;*/
    /*int lps_precision = LPS_PREC;*/

    /*log_info("calibrating delay loop...");*/
    /*while (loops_per_sec <<= 1)*/
    /*{*/
        /*// wait for "start of" clock tick*/
        /*ticks = jiffies;*/
        /*while (ticks == jiffies);*/
        /*// Go*/
        /*ticks = jiffies;*/
        /*do_delay(loops_per_sec);*/
        /*ticks = jiffies - ticks;*/
        /*if (ticks)*/
        /*{*/
            /*break;*/
        /*}*/
    /*}*/

    /*// Do a binary approximation to get loops_per_second set to equal one clock*/
    /*// (up to lps_precision bits)*/
    /*loops_per_sec >>= 1;*/
    /*loopbit = loops_per_sec;*/

    /*while (lps_precision-- && (loopbit >>= 1) )*/
    /*{*/
        /*loops_per_sec |= loopbit;*/
        /*ticks = jiffies;*/
        /*while (ticks == jiffies);*/
        /*ticks = jiffies;*/
        /*do_delay(loops_per_sec);*/
        /*if (jiffies != ticks) // longer than 1 tick*/
        /*{*/
            /*loops_per_sec &= ~loopbit;*/
        /*}*/
    /*}*/

    /*// finally, adjust loops per second in terms of seconds instead of clocks*/
    /*loops_per_sec *= HZ;*/
    /*// Round the value and print it*/

    /*log_info("ok - %lu.%02lu BogoMIPS",*/
        /*(loops_per_sec + 2500) / 500000,*/
        /*((loops_per_sec + 2500) / 5000) % 100);*/

    /*cpu_info.bogomips = loops_per_sec;*/

    if (!cpu_has(INTEL_TSC))
    {
        log_warning("tsc not available!");
        return;
    }

    log_info("detecting tsc...");

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
