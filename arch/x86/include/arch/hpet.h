#pragma once

#include <arch/acpi.h>
#include <kernel/clock.h>
#include <kernel/kernel.h>

// Reference: https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/software-developers-hpet-spec-1-0a.pdf

struct hpet;
struct hpet_acpi;
struct hpet_address;
struct hpet_capabilities;
struct hpet_configuration;
struct hpet_timer_register;

typedef struct hpet hpet_t;
typedef struct hpet_acpi hpet_acpi_t;
typedef struct hpet_address hpet_address_t;
typedef struct hpet_capabilities hpet_capabilities_t;
typedef struct hpet_configuration hpet_configuration_t;
typedef struct hpet_timer_register hpet_timer_register_t;

struct hpet_address
{
    volatile uint8_t type;
    volatile uint8_t register_bit_width;
    volatile uint8_t register_bit_offset;
    volatile uint8_t reserved;
    volatile uint64_t address;
} PACKED;

struct hpet_acpi
{
    sdt_header_t header;
    uint8_t hardware_rev_id;
    uint8_t comparator_count:5;
    uint8_t counter_size:1;
    uint8_t reserved:1;
    uint8_t legacy_replacement:1;
    uint16_t pci_vendor_id;
    hpet_address_t address;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
} PACKED;

struct hpet_capabilities
{
    volatile uint8_t rev_id;
    volatile uint8_t num_tim_cap:5;
    volatile uint8_t count_size_cap:1;
    volatile uint8_t reserved_1:1;
    volatile uint8_t leg_rt_cap:1;
    volatile uint16_t vendor_id;
    volatile uint32_t counter_clk_period;
    volatile uint64_t reserved_2;
} PACKED;

struct hpet_configuration
{
    volatile uint64_t enable_cnf:1;
    volatile uint64_t leg_rt_cnf:1;
    volatile uint64_t reserved:62;
} PACKED;

struct hpet_timer_register
{
    volatile uint8_t reserved_1:1;
    volatile uint8_t int_type:1;
    volatile uint8_t int_enable:1;
    volatile uint8_t type:1;
    volatile uint8_t periodic_mode:1;
    volatile uint8_t size:1;
    volatile uint8_t value_set:1;
    volatile uint8_t reserved_2:1;
    volatile uint8_t mode_32:1;
    volatile uint8_t int_routing:5;
    volatile uint8_t fsb_enable:1;
    volatile uint8_t fsb_del:1;
    volatile uint16_t reserved_3;
    volatile uint32_t routing_cap;
    volatile uint8_t padding[24];
} PACKED;

struct hpet
{
    hpet_capabilities_t cap;
    hpet_configuration_t config;
    uint8_t padding[216];
    volatile uint32_t main_counter_value;
    uint8_t padding_2[12];
    hpet_timer_register_t timers[0];
};

void hpet_initialize(void);
uint32_t hpet_freq_get(void);

extern hpet_t* hpet;
