#include <stdint.h>

enum
{
    MP_CPU        = 0,
    MP_BUS        = 1,
    MP_IOAPIC     = 2,
    MP_IOAPIC_IRQ = 3,
    MP_LAPIC_IRQ  = 4,

    MP_IRQ_TYPE_INT    = 0,
    MP_IRQ_TYPE_NMI    = 1,
    MP_IRQ_TYPE_SMI    = 2,
    MP_IRQ_TYPE_EXTINT = 3,
};

struct mp
{
    uint32_t signature;
    uint32_t mp_table;
    uint8_t  len;
    uint8_t  spec_rev;
    uint8_t  checksum;
    uint8_t  features[5];
};

typedef struct mp mp_t;

struct mp_table
{
    uint32_t signature;
    uint16_t len;
    uint8_t  spec_rev;
    uint8_t  checksum;
    char     oem_id[8];
    char     product_id[12];
    uint32_t oem_table_ptr;
    uint16_t oem_table_size;
    uint16_t entry_count;
    uint32_t lapic_ptr;
    uint16_t extended_table_len;
    uint8_t  extended_table_checksum;
    uint8_t  reserved;
};

typedef struct mp_table mp_table_t;

struct mp_cpu
{
    uint8_t  entry_type;
    uint8_t  lapic_id;
    uint8_t  lapic_ver;
    uint8_t  flags;
    uint32_t signature;
    uint32_t feature_flags;
    uint32_t reserved[2];
};

typedef struct mp_cpu mp_cpu_t;

struct mp_bus
{
    uint8_t entry_type;
    uint8_t id;
    char    type_string[6];
};

typedef struct mp_bus mp_bus_t;

struct mp_ioapic
{
    uint8_t  entry_type;
    uint8_t  id;
    uint8_t  ver;
    uint8_t  flags;
    uint32_t ptr;
};

typedef struct mp_ioapic mp_ioapic_t;

struct mp_ioapic_irq
{
    uint8_t  entry_type;
    uint8_t  type;
    uint16_t flags;
    uint8_t  src_bus_id;
    uint8_t  src_bus_irq;
    uint8_t  dest_ioapic_id;
    uint8_t  dest_ioapic_pin;
};

typedef struct mp_ioapic_irq mp_ioapic_irq_t;

struct mp_lapic_irq
{
    uint8_t  entry_type;
    uint8_t  type;
    uint16_t flags;
    uint8_t  src_bus_id;
    uint8_t  src_bus_irq;
    uint8_t  dest_lapic_id;
    uint8_t  dest_lapic_pin;
};

typedef struct mp_lapic_irq mp_lapic_irq_t;

void mp_read(void);
