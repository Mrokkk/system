#pragma once

#include <arch/cmos.h>
#include <kernel/rtc.h>

struct scheduled_event;

typedef struct scheduled_event event_t;

struct scheduled_event
{
    void (*handler)(void* data);
    void* data;
};

void rtc_read(rtc_meas_t* m);
void rtc_print(void);
void rtc_initialize(void);
void rtc_schedule(void (*handler)(void* data), void* data);
