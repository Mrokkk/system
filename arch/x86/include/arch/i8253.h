#pragma once

#include <stdint.h>
#include <arch/nmi.h>
#include <kernel/clock.h>

#define I8253_PORT_CHANNEL0 0x40
#define I8253_PORT_CHANNEL1 0x41
#define I8253_PORT_CHANNEL2 0x42
#define I8253_PORT_COMMAND  0x43

#define I8253_16BIN         (0 << 0)
#define I8253_BCD           (1 << 0)

#define I8253_MODE_0        (0 << 1)
#define I8253_MODE_1        (1 << 1)
#define I8253_MODE_2        (2 << 1)
#define I8253_MODE_3        (3 << 1)
#define I8253_MODE_4        (4 << 1)
#define I8253_MODE_5        (5 << 1)
#define I8253_MODE_2b       (6 << 1)
#define I8253_MODE_3b       (7 << 1)

#define I8253_LATCH_CVAL    (0 << 4)
#define I8253_ACCESS_LO     (1 << 4)
#define I8253_ACCESS_HI     (2 << 4)
#define I8253_ACCESS_LOHI   (3 << 4)

#define I8253_CHANNEL0      (0 << 6)
#define I8253_CHANNEL1      (1 << 6)
#define I8253_CHANNEL2      (2 << 6)
#define I8253_READ_BACK     (3 << 6)

void i8253_initialize();
void i8253_configure(uint8_t channel, uint8_t attr, uint16_t freq);
