#define log_fmt(fmt) "hpet: " fmt
#include <arch/acpi.h>
#include <arch/hpet.h>

#include <kernel/time.h>
#include <kernel/clock.h>
#include <kernel/kernel.h>
#include <kernel/page_mmio.h>

hpet_t* hpet;

static int hpet_enable(void);
static int hpet_disable(void);
static uint64_t hpet_read(void);

static clock_source_t hpet_clock = {
    .name = "hpet",
    .mask = 0xffffffff,
    .enable = &hpet_enable,
    .shutdown = &hpet_disable,
    .read = &hpet_read,
};

UNMAP_AFTER_INIT void hpet_initialize(void)
{
    static_assert(offsetof(hpet_t, cap) == 0);
    static_assert(offsetof(hpet_t, config) == 0x10);
    static_assert(offsetof(hpet_t, main_counter_value) == 0xf0);
    static_assert(offsetof(hpet_t, timers) == 0x100);

    hpet_acpi_t* sdt = acpi_find("HPET");
    uint32_t hpet_freq, mhz, mhz_remainder;

    if (!sdt)
    {
        log_notice("no HPET");
        return;
    }

    if (sdt->address.type == 1)
    {
        log_notice("only ISA port access");
        return;
    }

    hpet = mmio_map_uc(addr(sdt->address.address), PAGE_SIZE, "hpet");

    hpet_capabilities_t* cap = &hpet->cap;
    hpet_timer_register_t* timers = hpet->timers;

    mhz = hpet_freq = hpet_freq_get();
    hpet_clock.freq_khz = hpet_freq / KHz;
    mhz_remainder = do_div(mhz, MHz);

    log_notice("freq: %u.%04u MHz", mhz, mhz_remainder);

    for (int i = 0; i < cap->num_tim_cap + 1; ++i)
    {
        log_notice("timer[%u]: periodic mode: %s; IRQ routing cap: %x; %u bits",
            i,
            timers[i].periodic_mode ? "supported" : "unsupported",
            timers[i].routing_cap,
            timers[i].size ? 64 : 32);

        // If timer works in 64bit mode, force 32bit
        if (timers[i].size)
        {
            log_continue("; forcing 32 bits");
            timers[i].mode_32 = true;
        }
    }

    clock_source_register_khz(&hpet_clock, hpet_freq);
}

uint32_t hpet_freq_get(void)
{
    uint64_t freq = 1000000000000000LL;
    uint64_t hpet_clk_period = hpet->cap.counter_clk_period;
    do_div(freq, hpet_clk_period);
    return freq;
}

static int hpet_enable(void)
{
    hpet->config.enable_cnf = true;
    return 0;
}

static int hpet_disable(void)
{
    hpet->config.enable_cnf = false;
    return 0;
}

static uint64_t hpet_read(void)
{
    uint32_t cnt = hpet->main_counter_value;
    return cnt;
}
