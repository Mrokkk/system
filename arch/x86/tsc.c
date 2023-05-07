#define log_fmt(fmt) "tsc: " fmt
#include <arch/io.h>
#include <arch/tsc.h>
#include <arch/hpet.h>
#include <arch/i8253.h>
#include <arch/descriptor.h>

#include <kernel/cpu.h>
#include <kernel/time.h>
#include <kernel/kernel.h>

static uint64_t tsc_read(void);
static void tsc_calibrate_by_i8253(void);
static void tsc_calibrate_by_hpet(void);

#define TSC_HPET_CALIBRATION_LOOPS 20

static clock_source_t tsc_clock = {
    .name = "tsc",
    .mask = 0xffffffffffffffff,
    .read = &tsc_read,
};

UNMAP_AFTER_INIT void tsc_initialize(void)
{
    if (!cpu_has(X86_FEATURE_TSC))
    {
        log_info("not available");
        return;
    }

    if (!cpu_has(X86_FEATURE_INVTSC))
    {
        log_info("not invariant");
        return;
    }

    tsc_calibrate_by_i8253();
    tsc_calibrate_by_hpet();

    clock_source_register(&tsc_clock);
}

static uint64_t tsc_read(void)
{
    uint64_t cnt;
    rdtscll(cnt);
    return cnt;
}

UNMAP_AFTER_INIT static void tsc_calibrate_by_i8253(void)
{
    uint32_t cycles, mhz, mhz_remainder;
    uint64_t tsc_start, tsc_end;

    // Enable the gate (with speaker disabled)
    outb((inb(NMI_SC_PORT) & ~NMI_SC_SPKR_DAT_EN) | NMI_SC_TIM_CNT2_EN, NMI_SC_PORT);

    // Configure PIT channel 2 to mode 0, lo/hi, binary
    i8253_configure(I8253_CHANNEL2, I8253_MODE_0 | I8253_ACCESS_LOHI | I8253_16BIN, FREQ_100HZ);

    // Measure counter diff during 10 ms tick
    rdtscll(tsc_start);
    while ((inb(NMI_SC_PORT) & NMI_SC_TMR2_OUT_STS) == 0);
    rdtscll(tsc_end);

    mhz = cycles = (uint32_t)(tsc_end - tsc_start);
    mhz_remainder = do_div(mhz, MHz / FREQ_100HZ);
    tsc_clock.freq_khz = cycles / (KHz / FREQ_100HZ);

    log_notice("detected %u.%04u MHz [i8253 calibration]", mhz, mhz_remainder);
}

UNMAP_AFTER_INIT static void tsc_calibrate_by_hpet(void)
{
    uint32_t cycles, khz, mhz, mhz_remainder, hpet_max;
    uint64_t tsc_start, tsc_end;

    if (!hpet)
    {
        return;
    }

    hpet->config.enable_cnf = false;
    hpet_max = (hpet_freq_get() / FREQ_100HZ) * TSC_HPET_CALIBRATION_LOOPS + hpet->main_counter_value;

    hpet->config.enable_cnf = true;

    // Measure counter diff during 10 ms tick
    rdtscll(tsc_start);
    while (hpet->main_counter_value < hpet_max);
    rdtscll(tsc_end);

    hpet->config.enable_cnf = false;

    khz = mhz = cycles = ((uint32_t)(tsc_end - tsc_start)) / TSC_HPET_CALIBRATION_LOOPS;
    mhz_remainder = do_div(mhz, MHz / FREQ_100HZ);
    do_div(khz, KHz / FREQ_100HZ);
    tsc_clock.freq_khz = khz;

    log_notice("detected %u.%04u MHz [hpet calibration]", mhz, mhz_remainder);
}
