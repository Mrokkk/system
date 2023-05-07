#pragma once

#include <arch/cmos.h>

struct scheduled_event;

struct rtc_meas
{
    uint8_t second, minute, hour;
    uint8_t day, month, year;
};

typedef struct rtc_meas rtc_meas_t;
typedef struct scheduled_event event_t;

struct scheduled_event
{
    void (*handler)(void* data);
    void* data;
};

void rtc_read(rtc_meas_t* m);
void rtc_print(void);
void rtc_enable(void);
void rtc_disable(void);
void rtc_schedule(void (*handler)(void* data), void* data);
