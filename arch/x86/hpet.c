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

static uint32_t hpet_readl_native(uint32_t reg)
{
    void* h = hpet;
    return readl(h + reg);
}

static uint32_t hpet_readl_impl(uint32_t reg, uint32_t offset, uint32_t mask)
{
    void* h = hpet;

    uint32_t dword = offset >> 5;
    uint32_t dword_offset = offset % 32;
    uint32_t value = readl(h + reg + dword * 4);

    return (value >> dword_offset) & mask;
}

static void hpet_writel_impl(uint32_t reg, uint32_t offset, uint32_t mask, uint32_t value)
{
    void* h = hpet;

    uint32_t dword = offset >> 5;
    uint32_t dword_offset = offset % 32;
    void* iomem = h + reg + dword * 4;
    uint32_t orig_value = readl(iomem);

    orig_value &= ~(mask << dword_offset);
    orig_value |= value << dword_offset;

    writel(orig_value, iomem);
}

#define HPET_READL(reg) \
    ({ \
        hpet_readl_impl(HPET_##reg##_REG, HPET_##reg##_OFFSET, HPET_##reg##_MASK); \
    })

#define HPET_WRITEL(value, reg) \
    ({ \
        hpet_writel_impl(HPET_##reg##_REG, HPET_##reg##_OFFSET, HPET_##reg##_MASK, value); \
    })

#define HPET_TIMER_READL(reg, n) \
    ({ \
        hpet_readl_impl(HPET_##reg##_REG + 0x20 * n, HPET_##reg##_OFFSET, HPET_##reg##_MASK); \
    })

#define HPET_TIMER_WRITEL(value, reg, n) \
    ({ \
        hpet_writel_impl(HPET_##reg##_REG + 0x20 * n, HPET_##reg##_OFFSET, HPET_##reg##_MASK, value); \
    })

UNMAP_AFTER_INIT void hpet_initialize(void)
{
    hpet_acpi_t* sdt = acpi_find("HPET");
    uint32_t hpet_freq, mhz, mhz_remainder;

    if (!sdt)
    {
        log_notice("no HPET");
        return;
    }

    if (sdt->address.type == 1)
    {
        log_notice("no MMIO registers");
        return;
    }

    hpet = mmio_map_uc(addr(sdt->address.address), PAGE_SIZE, "hpet");

    if (HPET_READL(REV_ID) == 0)
    {
        log_notice("empty REV_ID");
        return;
    }

    mhz = hpet_freq = hpet_freq_get();
    hpet_clock.freq_khz = hpet_freq / KHz;
    mhz_remainder = do_div(mhz, MHz);

    size_t num_tim = HPET_READL(NUM_TIM_CAP) + 1;

    log_notice("freq: %u.%04u MHz; timers: %u", mhz, mhz_remainder, num_tim);

    HPET_WRITEL(0, ENABLE_CNF);

    for (size_t i = 0; i < num_tim; ++i)
    {
        bool is_64bit = HPET_TIMER_READL(Tn_SIZE_CAP, i);

        log_notice("timer[%u]: periodic mode: %s; IRQ routing cap: %#x; %u bits",
            i,
            HPET_TIMER_READL(Tn_PER_INT_CAP, i) ? "supported" : "unsupported",
            HPET_TIMER_READL(Tn_INT_ROUTE_CAP, i),
            is_64bit ? 64 : 32);

        HPET_TIMER_WRITEL(0, Tn_INT_ENB_CNF, i);

        // If timer works in 64bit mode, force 32bit
        if (is_64bit)
        {
            log_continue("; forcing 32 bits");
            HPET_TIMER_WRITEL(1, Tn_32MODE_CNF, i);
        }
    }

    clock_source_register_khz(&hpet_clock, hpet_freq);
}

uint32_t hpet_freq_get(void)
{
    uint64_t freq = 1000000000000000LL;
    uint64_t hpet_clk_period = HPET_READL(COUNTER_CLK_PERIOD);
    do_div(freq, hpet_clk_period);
    return freq;
}

int hpet_enable(void)
{
    HPET_WRITEL(1, ENABLE_CNF);
    return 0;
}

int hpet_disable(void)
{
    HPET_WRITEL(0, ENABLE_CNF);
    return 0;
}

static uint64_t hpet_read(void)
{
    return hpet_readl_native(HPET_REG_MAIN_COUNTER);
}

uint32_t hpet_cnt_value(void)
{
    return hpet_readl_native(HPET_REG_MAIN_COUNTER);
}
