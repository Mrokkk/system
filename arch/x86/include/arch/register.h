#pragma once

#include <arch/processor.h>
#include <kernel/compiler.h>

#define EFL_CF          (1 << 0)
#define EFL_PF          (1 << 2)
#define EFL_AF          (1 << 4)
#define EFL_ZF          (1 << 6)
#define EFL_SF          (1 << 7)
#define EFL_TF          (1 << 8)
#define EFL_IF          (1 << 9)
#define EFL_DF          (1 << 10)
#define EFL_OF          (1 << 11)
#define EFL_IOPL        (1 << 12)
#define EFL_NT          (1 << 14)
#define EFL_RF          (1 << 16)
#define EFL_VM          (1 << 17)
#define EFL_AC          (1 << 18)
#define EFL_VIF         (1 << 19)
#define EFL_VIP         (1 << 20)
#define EFL_ID          (1 << 21)

#define CR0_PE          (1 << 0)
#define CR0_MP          (1 << 1)
#define CR0_EM          (1 << 2)
#define CR0_TS          (1 << 3)
#define CR0_ET          (1 << 4)
#define CR0_NE          (1 << 5)
#define CR0_WP          (1 << 16)
#define CR0_AM          (1 << 18)
#define CR0_NW          (1 << 29)
#define CR0_CD          (1 << 30)
#define CR0_PG          (1 << 31)

#define CR4_VME         (1 << 0)
#define CR4_PVI         (1 << 1)
#define CR4_TSD         (1 << 2)
#define CR4_DE          (1 << 3)
#define CR4_PSE         (1 << 4)
#define CR4_PAE         (1 << 5)
#define CR4_MCE         (1 << 6)
#define CR4_PGE         (1 << 7)
#define CR4_PCE         (1 << 8)
#define CR4_OSFXSR      (1 << 9)
#define CR4_OSXMMEXCPT  (1 << 10)
#define CR4_VMXE        (1 << 13)
#define CR4_SMXE        (1 << 14)
#define CR4_PCIDE       (1 << 17)
#define CR4_OSXSAVE     (1 << 18)
#define CR4_SMEP        (1 << 20)
#define CR4_SMAP        (1 << 21)

#ifndef __ASSEMBLER__

#include <stddef.h>
#include <kernel/printk.h>

#define general_reg_union(letter) \
    { \
        uint32_t e##letter##x; \
        uint16_t letter##x; \
        struct \
        { \
            uint8_t letter##l; \
            uint8_t letter##h; \
        }; \
    }

#define index_reg_union(letter) \
    { \
        uint32_t e##letter##i; \
        uint16_t letter##i; \
    }

struct regs
{
    union general_reg_union(a);
    union general_reg_union(b);
    union general_reg_union(c);
    union general_reg_union(d);
    union index_reg_union(s);
    union index_reg_union(d);
    uint32_t eflags;
} PACKED;

typedef struct regs regs_t;

void regs_print(loglevel_t severity, const pt_regs_t* regs, const char* header);

const char* eflags_bits_string_get(uint32_t eflags, char* buffer, size_t size);
const char* cr0_bits_string_get(uint32_t cr0, char* buffer, size_t size);
const char* cr4_bits_string_get(uint32_t cr4, char* buffer, size_t size);

extern uint32_t eip_get(void);

static inline uint32_t dr0_get(void)
{
    uint32_t rv;
    asm volatile("mov %%dr0, %0" : "=r" (rv));
    return rv;
}

static inline uint32_t dr1_get(void)
{
    uint32_t rv;
    asm volatile("mov %%dr2, %0" : "=r" (rv));
    return rv;
}

static inline uint32_t dr2_get(void)
{
    uint32_t rv;
    asm volatile("mov %%dr2, %0" : "=r" (rv));
    return rv;
}

static inline uint32_t dr3_get(void)
{
    uint32_t rv;
    asm volatile("mov %%dr3, %0" : "=r" (rv));
    return rv;
}

#define eflags_get() \
    ({ \
        uint32_t rv; \
        asm volatile("pushf; pop %0" : "=r" (rv)); \
        rv; \
    })

#define cr0_get() \
    ({ \
        uint32_t rv; \
        asm volatile("mov %%cr0, %0" : "=r" (rv)); \
        rv; \
    })

#define cr2_get() \
    ({ \
        uint32_t rv; \
        asm volatile("mov %%cr2, %0" : "=r" (rv)); \
        rv; \
    })

#define cr3_get() \
    ({ \
        uint32_t rv; \
        asm volatile("mov %%cr3, %0" : "=r" (rv)); \
        rv; \
    })

#define cr4_get() \
    ({ \
        uint32_t rv; \
        asm volatile("mov %%cr4, %0" : "=r" (rv)); \
        rv; \
    })

#define cs_get() \
    ({ \
        uint32_t rv; \
        asm volatile("mov %%cs, %0" : "=r" (rv)); \
        rv; \
    })

#define ds_get() \
    ({ \
        uint32_t rv; \
        asm volatile("mov %%ds, %0" : "=r" (rv)); \
        rv; \
    })

#define es_get() \
    ({ \
        uint32_t rv; \
        asm volatile("mov %%es, %0" : "=r" (rv)); \
        rv; \
    })

#define fs_get() \
    ({ \
        uint32_t rv; \
        asm volatile("mov %%fs, %0" : "=r" (rv)); \
        rv; \
    })

#define gs_get() \
    ({ \
        uint32_t rv; \
        asm volatile("mov %%gs, %0" : "=r" (rv)); \
        rv; \
    })

#define ss_get() \
    ({ \
        uint32_t rv; \
        asm volatile("mov %%ss, %0" : "=r" (rv)); \
        rv; \
    })

#define esp_get() \
    ({ \
        uint32_t rv; \
        asm volatile("mov %%esp, %0" : "=r" (rv)); \
        rv; \
    })

#define ebp_get() \
    ({ \
        uint32_t rv; \
        asm volatile("mov %%ebp, %0" : "=r" (rv)); \
        rv; \
    })

#endif // __ASSEMBLER__
