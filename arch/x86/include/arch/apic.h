#pragma once

#include <arch/io.h>
#include <arch/acpi.h>
#include <kernel/clock.h>

// https://coconucos.cs.uni-duesseldorf.de/lehre/abschlussarbeiten/BA-Urlacher-23.pdf

struct apic;
struct madt;
struct ioapic;
struct madt_entry;
struct ioapic_route;

typedef struct apic apic_t;
typedef struct madt madt_t;
typedef struct ioapic ioapic_t;
typedef struct madt_entry madt_entry_t;
typedef struct ioapic_route ioapic_route_t;

#define IA32_APIC_BASE_MSR          0x1b
#define IA32_APIC_BASE_MSR_BSP      0x100
#define IA32_APIC_BASE_MSR_ENABLE   0x800

#define APIC_BASE_MASK              0xfffff000

#define APIC_SIV_ENABLE                (1 << 8)
#define APIC_LVT_INT_MASKED            (1 << 16)
#define APIC_LVT_TIMER_PERIODIC        (1 << 17)
#define APIC_ICR_SEND_PENDING          (1 << 12)
#define APIC_ICR_DELIVERY_START_UP     (6 << 8)
#define APIC_ICR_DELIVERY_INIT         (5 << 8)
#define APIC_ICR_LEVEL_ASSERT          (1 << 14)
#define APIC_ICR_LEVEL_DEASSERT        (0 << 14)
#define APIC_ICR_TRIGGER_LEVEL         (1 << 15)
#define APIC_ICR_TRIGGER_EDGE          (0 << 15)
#define APIC_ICR_DEST_SELF             (1 << 18)
#define APIC_ICR_DEST_ALL              (2 << 18)
#define APIC_ICR_DEST_ALL_SELF_EXCLUDE (3 << 18)
#define APIC_TIMER DIV_1               0xb
#define APIC_TIMER_DIV_2               0x0
#define APIC_TIMER_DIV_4               0x1
#define APIC_TIMER_DIV_8               0x2
#define APIC_TIMER_DIV_16              0x3
#define APIC_TIMER_DIV_32              0x8
#define APIC_TIMER_DIV_64              0x9
#define APIC_TIMER_DIV_128             0xa
#define APIC_TIMER_MAXCNT              (~(uint32_t)0)

#define APIC_SMP_NOTIF_VECTOR          253
#define APIC_ERROR_IRQ_VECTOR          254
#define APIC_SPURIOUS_IRQ_VECTOR       255

#define IOAPIC_REG_REDIRECT            0x10

struct apic
{
    io32                    __1[8];
    io32 id,                __2[3];
    io8 version;
    io8 reserved;
    io8 max_lvt_entry,      __4;
    io32                    __5[31];
    io32 eoi,               __6[15];
    io32 siv,               __7[99];
    io32 esr,               __8[31];
    io32 icr_low,           __9[3];
    io32 icr_hi,            __10[3];
    io32 lvt_timer,         __11[11];
    io32 lvt_lint0,         __12[3];
    io32 lvt_lint1,         __13[3];
    io32 lvt_error,         __14[3];
    io32 timer_init_cnt,    __15[3];
    io32 timer_current_cnt, __16[19];
    io32 timer_div;
} PACKED;

struct madt
{
    sdt_header_t header;
    uint32_t lapic_address;
    uint32_t flags;
} PACKED;

#define MADT_TYPE_LAPIC                 0
#define MADT_TYPE_IOAPIC                1
#define MADT_TYPE_IOAPIC_OVERRIDE       2
#define MADT_TYPE_LAPIC_NMI             4
#define MADT_TYPE_LAPIC_ADDR_OVERRIDE   5
#define MADT_TYPE_X2LAPIC               9

struct madt_entry
{
    uint8_t type;
    uint8_t len;
    union
    {
        struct
        {
            uint8_t cpu_id;
            uint8_t apic_id;
            uint32_t flags;
        } PACKED lapic;
        struct
        {
            uint8_t id;
            uint8_t reserved;
            uint32_t address;
            uint32_t gsi;
        } PACKED ioapic;
        struct
        {
            uint8_t bus;
            uint8_t irq;
            uint32_t gsi;
            uint16_t flags;
        } PACKED ioapic_override;
        struct
        {
            uint8_t cpu_id;
            uint16_t flags;
            uint8_t lint;
        } PACKED lapic_nmi;
        struct
        {
            uint16_t reserved;
            uint64_t address;
        } PACKED lapic_addr_override;
        struct
        {
            uint16_t reserved;
            uint32_t x2apic_id;
            uint32_t flags;
            uint32_t apic_id;
        } PACKED x2apic;
    } PACKED;
} PACKED;

struct ioapic
{
    io32 index;
    io32 unused[3];
    io32 data;
    io32 unused2[11];
    io32 eoi;
} PACKED;

struct ioapic_route
{
    uint8_t interrupt_vector;
    uint8_t type:3;
    uint8_t destination_mode:1;
    uint8_t busy:1;
    uint8_t polarity:1;
    uint8_t eoi:1;
    uint8_t trigger_mode:1;
    uint8_t mask:1;
    uint64_t reserved:39;
    uint8_t destination;
} PACKED;

void apic_initialize(void);
void apic_ap_initialize(void);
void apic_timer_initialize(void);
void apic_eoi(uint32_t);
void apic_ipi_send(uint8_t lapic_id, uint32_t value);

extern apic_t* apic;
