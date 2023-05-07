#define log_fmt(fmt) "rtc: " fmt
#include <arch/nmi.h>
#include <arch/rtc.h>
#include <kernel/irq.h>
#include <kernel/time.h>
#include <kernel/kernel.h>

// HP ProBook 6470b: if RTC is enabled and power button is pressed to shutdown laptop, it turns on after
// couple of seconds

#define BCD_CONVERT(v) \
    v = ((v & 0xf0) >> 1) + ((v & 0xf0) >> 3) + (v & 0xf)

#define RTC_RATE    RTC_FREQ_64Hz

static event_t events[2];

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
}

static void rtc_irq()
{
    if (events[0].handler)
    {
        events[0].handler(events[0].data);
    }
}

static inline void rtc_eoi(void)
{
    cmos_read(CMOS_RTC_REGISTER_C);
}

void rtc_enable(void)
{
    uint8_t data;

    scoped_irq_lock();

    data = cmos_read(CMOS_RTC_REGISTER_A);
    cmos_write((data & 0xf0) | RTC_RATE, CMOS_RTC_REGISTER_A);

    data = cmos_read(CMOS_RTC_REGISTER_B);
    cmos_write(data | RTC_PIE, CMOS_RTC_REGISTER_B);

    irq_register(8, &rtc_irq, "rtc", IRQ_DEFAULT);

    rtc_eoi();
}

void rtc_disable(void)
{
    uint8_t data = cmos_read(CMOS_RTC_REGISTER_B);
    cmos_write(data & ~RTC_PIE, CMOS_RTC_REGISTER_B);
}

void rtc_schedule(void (*handler)(void* data), void* data)
{
    scoped_irq_lock();
    events[0].handler = handler;
    events[0].data = data;
    rtc_eoi();
}

void rtc_print(void)
{
    rtc_meas_t m;
    rtc_read(&m);
    log_notice("%02u-%02u-%u %02u:%02u:%02u %u", m.day, m.month, m.year, m.hour, m.minute, m.second,
        mktime(m.year + 2000, m.month, m.day, m.hour, m.minute, m.second));
}

time_t sys_time(void*)
{
    rtc_meas_t m;
    rtc_read(&m);
    return mktime(m.year + 2000, m.month, m.day, m.hour, m.minute, m.second);
}
