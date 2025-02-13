#define log_fmt(fmt) "hpet: " fmt
#include <arch/io.h>
#include <arch/acpi.h>
#include <arch/hpet.h>

#include <kernel/time.h>
#include <kernel/clock.h>
#include <kernel/kernel.h>
#include <kernel/page_mmio.h>

void* hpet;

static uint64_t hpet_read(void);

static clock_source_t hpet_clock = {
    .name = "hpet",
    .mask = 0xffffffff,
    .enable = &hpet_enable,
    .shutdown = &hpet_disable,
    .read = &hpet_read,
};

static inline uint32_t hpet_readl(uint32_t reg)
{
    return readl(hpet + reg);
}

static inline void hpet_writel(uint32_t reg, uint32_t val)
{
    writel(val, hpet + reg);
}

static inline uint64_t hpet_readq(uint32_t reg)
{
    return readq(hpet + reg);
}

bool hpet_available(void)
{
    return !!hpet;
}

UNMAP_AFTER_INIT void hpet_initialize(void)
{
    hpet_acpi_t* sdt = acpi_find("HPET");

    if (unlikely(!sdt))
    {
        log_notice("no HPET");
        return;
    }

    if (unlikely(sdt->address.type == 1))
    {
        log_notice("no MMIO registers");
        return;
    }

    hpet = mmio_map_uc(addr(sdt->address.address), PAGE_SIZE, "hpet");

    if (unlikely(!hpet))
    {
        log_notice("cannot map MMIO");
        return;
    }

    uint64_t general_cap = hpet_readq(HPET_REG_GENERAL_CAP);
    uint32_t general_config = hpet_readl(HPET_REG_GENERAL_CONFIG);

    uint8_t rev_id = REV_ID_GET(general_cap);
    uint16_t vendor_id = VENDOR_ID_GET(general_cap);
    uint32_t counter_clk_period = COUNTER_CLK_PERIOD_GET(general_cap);

    if (unlikely(rev_id == 0))
    {
        log_notice("empty REV_ID");
        return;
    }

    if (unlikely(!counter_clk_period || counter_clk_period > 0x05f5e100))
    {
        log_notice("invalid COUNTER_CLK_PERIOD: %#x", counter_clk_period);
        return;
    }

    uint32_t hpet_freq, mhz, mhz_remainder;

    mhz = hpet_freq = hpet_freq_get();
    hpet_clock.freq_khz = hpet_freq / KHz;
    mhz_remainder = do_div(mhz, MHz);

    size_t num_tim = NUM_TIM_CAP_GET(general_cap) + 1;

    log_notice("vendor: %#x; rev: %#x; freq: %u.%04u MHz; timers: %u; legacy replacement: %s",
        vendor_id,
        rev_id,
        mhz,
        mhz_remainder,
        num_tim,
        (general_cap & LEG_RT_CAP) ? "true" : "false");

    hpet_writel(HPET_REG_GENERAL_CONFIG, general_config & ~ENABLE_CNF);

    for (size_t i = 0; i < num_tim; ++i)
    {
        uint64_t tn_config = hpet_readq(TIMER_N_CONFIG(i));

        log_notice("timer[%u]: periodic: %s; IRQ routing: %#x; %u bits; FSB: %s",
            i,
            tn_config & Tn_PER_INT_CAP ? "true" : "false",
            (uint32_t)Tn_INT_ROUTE_CAP_GET(tn_config),
            tn_config & Tn_SIZE_CAP ? 64 : 32,
            tn_config & Tn_FSB_INT_DEL_CAP ? "true" : "false");

        // If timer works in 64bit mode, force 32bit
        if (tn_config & Tn_SIZE_CAP)
        {
            tn_config |= Tn_32MODE_CNF;
        }

        tn_config &= ~Tn_INT_ENB_CNF;

        hpet_writel(TIMER_N_CONFIG(i), tn_config);
    }

    clock_source_register_khz(&hpet_clock, hpet_freq);
}

uint32_t hpet_freq_get(void)
{
    uint64_t freq = 1000000000000000ULL;
    uint64_t hpet_clk_period = COUNTER_CLK_PERIOD_GET(hpet_readq(HPET_REG_GENERAL_CAP));
    do_div(freq, hpet_clk_period);
    return freq;
}

int hpet_enable(void)
{
    hpet_writel(
        HPET_REG_GENERAL_CONFIG,
        hpet_readl(HPET_REG_GENERAL_CONFIG) | ENABLE_CNF);
    return 0;
}

int hpet_disable(void)
{
    hpet_writel(
        HPET_REG_GENERAL_CONFIG,
        hpet_readl(HPET_REG_GENERAL_CONFIG) & ~ENABLE_CNF);
    return 0;
}

static uint64_t hpet_read(void)
{
    return hpet_readl(HPET_REG_MAIN_COUNTER);
}

uint32_t hpet_cnt_value(void)
{
    return hpet_readl(HPET_REG_MAIN_COUNTER);
}
