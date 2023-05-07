#pragma once

#include <arch/io.h>

#define CMOS_REGISTER       0x70
#define CMOS_DATA           0x71

#define CMOS_RTC_SECONDS    0x00
#define CMOS_RTC_MINUTES    0x02
#define CMOS_RTC_HOURS      0x04
#define CMOS_RTC_WEEKDAY    0x06
#define CMOS_RTC_DAYOFMONTH 0x07
#define CMOS_RTC_MONTH      0x08
#define CMOS_RTC_YEAR       0x09
#define CMOS_RTC_REGISTER_A 0x0a
#define      RTC_UIP        (1 << 7)
#define      RTC_FREQ_64Hz  10
#define      RTC_FREQ_32Hz  11
#define      RTC_FREQ_16Hz  12
#define      RTC_FREQ_8Hz   13
#define CMOS_RTC_REGISTER_B 0x0b
#define      RTC_DM_BINARY  (1 << 2)
#define      RTC_PIE        (1 << 6)
#define CMOS_RTC_REGISTER_C 0x0c

#define CMOS_DISABLE_NMI    0x80

#define CMOS_DELAY() io_delay(1)

static inline uint8_t cmos_read(uint16_t reg)
{
    uint8_t data;
    outb(reg, CMOS_REGISTER);
    CMOS_DELAY();
    data = inb(CMOS_DATA);
    CMOS_DELAY();
    return data;
}

static inline void cmos_write(uint8_t data, uint16_t reg)
{
    outb(reg, CMOS_REGISTER);
    CMOS_DELAY();
    outb(data, CMOS_DATA);
    CMOS_DELAY();
}
