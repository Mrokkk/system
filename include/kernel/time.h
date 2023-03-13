#pragma once

#include <stdint.h>

#define HZ 100

void do_delay();
void delay(uint32_t);
void delay_calibrate(void);

extern uint32_t loops_per_sec;

static inline void udelay(uint32_t usecs)
{
    usecs *= loops_per_sec / 1000000;
    do_delay(usecs);
}
