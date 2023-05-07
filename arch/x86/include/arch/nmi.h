#pragma once

#include <arch/cmos.h>

#define NMI_SC_PORT         0x61
#define NMI_SC_TIM_CNT2_EN  (1 << 0)
#define NMI_SC_SPKR_DAT_EN  (1 << 1)
#define NMI_SC_TMR2_OUT_STS (1 << 5)
#define NMI_SC_MASK         (~((1 << 4) | (1 << 5) | (1 << 6) | (1 << 7)))

static inline void nmi_enable(void)
{
    outb(inb(CMOS_REGISTER) & (CMOS_DISABLE_NMI - 1), CMOS_REGISTER);
    inb(CMOS_DATA);
}

static inline void nmi_disable(void)
{
    outb(inb(CMOS_REGISTER) | CMOS_DISABLE_NMI, CMOS_REGISTER);
    inb(CMOS_DATA);
}
