#define log_fmt(fmt) "i8253: " fmt
#include <arch/io.h>
#include <arch/i8253.h>
#include <arch/descriptor.h>

#include <kernel/irq.h>
#include <kernel/time.h>
#include <kernel/clock.h>

#define CLOCK_TICK_RATE     1193182U
#define LATCH               ((CLOCK_TICK_RATE) / HZ)

// Interesting thing: if channel 1 is used on IBM ThinkPad T42, reboot and shutdown are
// freezing the CPU. Shutdown via APM is powering off display, drives, but computer is still
// running. Also, plugging in/out USB stick is causing a couple seconds freeze.
// According to ICH4-M documentation, channel 1 is used for refresh request signal

#define I8253_MONOTONIC_PORT  I8253_PORT_CHANNEL2
#define I8253_MONITONIC_ATTR  (I8253_MODE_2 | I8253_ACCESS_LOHI | I8253_16BIN)
#define I8253_MONOTONIC_LATCH (0xffff)

extern void timer_handler(void);
static int i8253_irq_enable(void);
static int i8253_enable(void);
static int i8253_disable(void);
static uint64_t i8253_read(void);

static clock_source_t i8253_clock = {
    .name = "i8253",
    .freq_khz = CLOCK_TICK_RATE / KHz,
    .mask = I8253_MONOTONIC_LATCH,
    .enable_systick = &i8253_irq_enable,
    .enable = &i8253_enable,
    .shutdown = &i8253_disable,
    .read = &i8253_read,
};

UNMAP_AFTER_INIT void i8253_initialize(void)
{
    clock_source_register(&i8253_clock);
}

UNMAP_AFTER_INIT static int i8253_irq_enable(void)
{
    outb(I8253_CHANNEL0 | I8253_MODE_2 | I8253_ACCESS_LOHI | I8253_16BIN, I8253_PORT_COMMAND);
    outb(LATCH & 0xff, I8253_PORT_CHANNEL0);
    outb(LATCH >> 8, I8253_PORT_CHANNEL0);
    irq_register(0, &timer_handler, "i8253", IRQ_NAKED | IRQ_ENABLE);
    return 0;
}

UNMAP_AFTER_INIT static int i8253_enable(void)
{
    uint8_t nmi_sc;

    outb(I8253_CHANNEL2 | I8253_MONITONIC_ATTR, I8253_PORT_COMMAND);
    outb(I8253_MONOTONIC_LATCH & 0xff, I8253_PORT_CHANNEL2);
    outb(I8253_MONOTONIC_LATCH >> 8, I8253_PORT_CHANNEL2);

    nmi_sc = inb(NMI_SC_PORT);

    if (!(nmi_sc & NMI_SC_TIM_CNT2_EN))
    {
        outb((NMI_SC_TIM_CNT2_EN | nmi_sc) & NMI_SC_MASK, NMI_SC_PORT);
    }

    return 0;
}

static int i8253_disable(void)
{
    outb(0x30, I8253_PORT_COMMAND);
    outb(0x0, I8253_PORT_CHANNEL0);
    outb(0x0, I8253_PORT_CHANNEL0);
    return 0;
}

static uint64_t i8253_read(void)
{
    uint16_t cnt;

    outb(0, I8253_PORT_COMMAND);
    cnt = inb(I8253_MONOTONIC_PORT);
    cnt |= inb(I8253_MONOTONIC_PORT) << 8;
    cnt = I8253_MONOTONIC_LATCH - cnt;

    return cnt;
}

UNMAP_AFTER_INIT void i8253_configure(uint8_t channel, uint8_t attr, uint16_t freq)
{
    uint8_t port;
    uint16_t latch;

    switch (channel)
    {
        case I8253_CHANNEL0:
            port = I8253_PORT_CHANNEL0;
            break;
        case I8253_CHANNEL1:
            port = I8253_PORT_CHANNEL1;
            break;
        case I8253_CHANNEL2:
            port = I8253_PORT_CHANNEL2;
            break;
        default: return;
    }

    latch = CLOCK_TICK_RATE / freq;

    outb(channel | attr, I8253_PORT_COMMAND);
    outb(latch & 0xff, port);
    outb(latch >> 8, port);
}
