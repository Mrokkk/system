#pragma once

#include <stdint.h>
#include <arch/register.h>

#define U32(a, b, c, d) \
    ((a << 0) + (b << 8) + (c << 16) + (d << 24))

#define BIOS32_SIGNATURE    U32('_', '3', '2', '_')

struct bios32_entry;
struct bios32_header;

typedef struct bios32_entry bios32_entry_t;
typedef struct bios32_header bios32_header_t;

struct bios32_header
{
    uint32_t signature;
    uint32_t entry;
    uint8_t revision;
    uint8_t len;
    uint8_t checksum;
    uint8_t reserved[5];
} PACKED;

struct bios32_entry
{
    uint32_t addr;
    uint16_t seg;
};

int bios32_init(void);
int bios32_find(uint32_t service, bios32_entry_t* entry);

static inline void bios32_call(struct bios32_entry* entry, regs_t* regs)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wuninitialized"
    asm volatile(
        "lcall *(%%esi);"
        : "=a" (regs->eax),
          "=b" (regs->ebx),
          "=c" (regs->ecx),
          "=d" (regs->edx),
          "=D" (regs->edi)
        : "a" (regs->eax),
          "b" (regs->ebx),
          "c" (regs->ecx),
          "d" (regs->edx),
          "S" (entry),
          "D" (regs->edi)
        : "cc", "memory"
    );
#pragma GCC diagnostic pop
}
