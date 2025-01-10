#pragma once

#include <arch/cmos.h>
#include <kernel/rtc.h>
#include <kernel/list.h>

void rtc_read(rtc_meas_t* m);
void rtc_print(void);
void rtc_initialize(void);
int rtc_event_schedule(void (*handler)(void* data), void* data);
void rtc_event_cancel(int id);
