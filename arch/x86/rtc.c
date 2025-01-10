#define log_fmt(fmt) "rtc: " fmt
#include <arch/nmi.h>
#include <arch/rtc.h>
#include <kernel/irq.h>
#include <kernel/list.h>
#include <kernel/time.h>
#include <kernel/clock.h>
#include <kernel/kernel.h>

// HP ProBook 6470b: if RTC is enabled and power button is pressed to shutdown laptop, it turns on after
// couple of seconds

#define BCD_CONVERT(v) \
    v = ((v & 0xf0) >> 1) + ((v & 0xf0) >> 3) + (v & 0xf)

#define RTC_TICK    32768
#define RTC_RATE    RTC_FREQ_64Hz

struct scheduled_event
{
    int         id;
    void*       data;
    list_head_t list_entry;

    void (*handler)(void* data);
};

typedef struct scheduled_event event_t;

static int rtc_enable(void);
static int rtc_disable(void);

static LIST_DECLARE(events);
static int lastId;

static clock_source_t rtc = {
    .name = "rtc",
    .freq_khz = RTC_TICK / KHz,
    .enable = &rtc_enable,
    .read_rtc = &rtc_read,
    .shutdown = &rtc_disable,
};

static inline bool cmos_rtc_is_updating()
{
    return cmos_read(CMOS_RTC_REGISTER_A) & RTC_UIP;
}

void rtc_read(rtc_meas_t* m)
{
    uint8_t status;
    flags_t flags;

    irq_save(flags);

    while (cmos_rtc_is_updating());

    status = cmos_read(CMOS_RTC_REGISTER_B);
    m->second = cmos_read(CMOS_RTC_SECONDS);
    m->minute = cmos_read(CMOS_RTC_MINUTES);
    m->hour = cmos_read(CMOS_RTC_HOURS);
    m->day = cmos_read(CMOS_RTC_DAYOFMONTH);
    m->month = cmos_read(CMOS_RTC_MONTH);
    m->year = cmos_read(CMOS_RTC_YEAR);

    irq_restore(flags);

    if (!(status & RTC_DM_BINARY))
    {
        BCD_CONVERT(m->second);
        BCD_CONVERT(m->minute);
        BCD_CONVERT(m->hour);
        BCD_CONVERT(m->day);
        BCD_CONVERT(m->month);
        BCD_CONVERT(m->year);
    }

    m->year += 2000;
}

static void rtc_irq()
{
    event_t* event;

    list_for_each_entry_safe(event, &events, list_entry)
    {
        event->handler(event->data);
        list_del(&event->list_entry);
        delete(event);
    }
}

static inline void rtc_eoi(void)
{
    cmos_read(CMOS_RTC_REGISTER_C);
}

void rtc_initialize(void)
{
    clock_source_register_hz(&rtc, RTC_TICK);
    irq_register(8, &rtc_irq, "rtc", IRQ_DEFAULT);
}

static int rtc_disable(void)
{
    uint8_t data = cmos_read(CMOS_RTC_REGISTER_B);
    cmos_write(data & ~RTC_PIE, CMOS_RTC_REGISTER_B);
    return 0;
}

int rtc_event_schedule(void (*handler)(void* data), void* data)
{
    scoped_irq_lock();
    event_t* event = alloc(event_t);

    if (unlikely(!event))
    {
        return -ENOMEM;
    }

    event->handler = handler;
    event->data = data;
    event->id = ++lastId;
    list_init(&event->list_entry);
    list_add(&event->list_entry, &events);

    rtc_eoi();

    return event->id;
}

void rtc_event_cancel(int id)
{
    event_t* event;
    scoped_irq_lock();
    list_for_each_entry(event, &events, list_entry)
    {
        if (event->id == id)
        {
            list_del(&event->list_entry);
            delete(event);
            return;
        }
    }
}

void rtc_print(void)
{
    rtc_meas_t m;
    rtc_read(&m);
    log_notice("%02u-%02u-%u %02u:%02u:%02u %u", m.day, m.month, m.year, m.hour, m.minute, m.second,
        mktime(m.year, m.month, m.day, m.hour, m.minute, m.second));
}

static int rtc_enable(void)
{
    uint8_t data;

    scoped_irq_lock();

    data = cmos_read(CMOS_RTC_REGISTER_A);
    cmos_write((data & 0xf0) | RTC_RATE, CMOS_RTC_REGISTER_A);

    data = cmos_read(CMOS_RTC_REGISTER_B);
    cmos_write(data | RTC_PIE, CMOS_RTC_REGISTER_B);

    rtc_eoi();

    return 0;
}
