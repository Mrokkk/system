#pragma once

#include <stdint.h>

typedef struct rtc_meas rtc_meas_t;

struct rtc_meas
{
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
};
