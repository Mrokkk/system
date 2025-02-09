#pragma once

#include <arch/io.h>
#include <arch/acpi.h>
#include <kernel/clock.h>
#include <kernel/kernel.h>

// Reference:
// https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/software-developers-hpet-spec-1-0a.pdf

typedef struct hpet_acpi hpet_acpi_t;
typedef struct hpet_address hpet_address_t;

struct hpet_address
{
    io8  type;
    io8  register_bit_width;
    io8  register_bit_offset;
    io8  reserved;
    io64 address;
} PACKED;

struct hpet_acpi
{
    sdt_header_t   header;
    uint8_t        hardware_rev_id;
    uint8_t        comparator_count:5;
    uint8_t        counter_size:1;
    uint8_t        reserved:1;
    uint8_t        legacy_replacement:1;
    uint16_t       pci_vendor_id;
    hpet_address_t address;
    uint8_t        hpet_number;
    uint16_t       minimum_tick;
    uint8_t        page_protection;
} PACKED;

#define HPET_VALUE(name, offset, mask, len, reg) \
    HPET_##name##_REG              = reg, \
    HPET_##name##_LEN    = len, \
    HPET_##name##_OFFSET = offset, \
    HPET_##name##_MASK   = mask,

enum hpet_registers
{
    HPET_REG_GENERAL_CAP = 0x000,

    HPET_VALUE(COUNTER_CLK_PERIOD, 32, 0xffffffff, 4, HPET_REG_GENERAL_CAP)
    HPET_VALUE(VENDOR_ID,          16, 0xffff,     2, HPET_REG_GENERAL_CAP)
    HPET_VALUE(LEG_RT_CAP,         15, 0x1,        1, HPET_REG_GENERAL_CAP)
    HPET_VALUE(COUNT_SIZE_CAP,     13, 0x1,        1, HPET_REG_GENERAL_CAP)
    HPET_VALUE(NUM_TIM_CAP,        8,  0x1f,       1, HPET_REG_GENERAL_CAP)
    HPET_VALUE(REV_ID,             0,  0xff,       1, HPET_REG_GENERAL_CAP)

    HPET_REG_GENERAL_CONFIG = 0x010,

    HPET_VALUE(LEG_RT_CNF, 1, 0x1, 1, HPET_REG_GENERAL_CONFIG)
    HPET_VALUE(ENABLE_CNF, 0, 0x1, 1, HPET_REG_GENERAL_CONFIG)

    HPET_REG_GENERAL_STATUS = 0x020,
    HPET_REG_MAIN_COUNTER   = 0x0f0,
    HPET_REG_TIMER_N_CONFIG = 0x100,

    HPET_VALUE(Tn_INT_ROUTE_CAP,   32, 0xffffffff, 4, HPET_REG_TIMER_N_CONFIG)
    HPET_VALUE(Tn_FSB_INT_DEL_CAP, 15, 0x1,        1, HPET_REG_TIMER_N_CONFIG)
    HPET_VALUE(Tn_FSB_EN_CNF,      14, 0x1,        1, HPET_REG_TIMER_N_CONFIG)
    HPET_VALUE(Tn_INT_ROUTE_CNF,   9,  0x1f,       1, HPET_REG_TIMER_N_CONFIG)
    HPET_VALUE(Tn_32MODE_CNF,      8,  0x1,        1, HPET_REG_TIMER_N_CONFIG)
    HPET_VALUE(Tn_VAL_SET_CNF,     6,  0x1,        1, HPET_REG_TIMER_N_CONFIG)
    HPET_VALUE(Tn_SIZE_CAP,        5,  0x1,        1, HPET_REG_TIMER_N_CONFIG)
    HPET_VALUE(Tn_PER_INT_CAP,     4,  0x1,        1, HPET_REG_TIMER_N_CONFIG)
    HPET_VALUE(Tn_TYPE_CNF,        3,  0x1,        1, HPET_REG_TIMER_N_CONFIG)
    HPET_VALUE(Tn_INT_ENB_CNF,     2,  0x1,        1, HPET_REG_TIMER_N_CONFIG)
    HPET_VALUE(Tn_INT_TYPE_CNF,    1,  0x1,        1, HPET_REG_TIMER_N_CONFIG)

    HPET_REG_TIMER_N_COMPARATOR = 0x108,
    HPET_REG_TIMER_N_FSB_ROUTE  = 0x110,
};

void hpet_initialize(void);
uint32_t hpet_freq_get(void);
int hpet_enable(void);
int hpet_disable(void);
uint32_t hpet_cnt_value(void);

extern void* hpet;
