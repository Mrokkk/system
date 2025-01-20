#pragma once

#define IA32_MSR_EFER                   0xc0000080
#define IA32_MSR_EFER_SCE               (1 << 0)
#define IA32_MSR_EFER_LME               (1 << 8)
#define IA32_MSR_EFER_NXE               (1 << 11)

#define IA32_MSR_MTRRCAP                0xfe
#define IA32_MSR_MTRRCAP_WC             (1 << 10)
#define IA32_MSR_MTRRCAP_FIX            (1 << 8)
#define IA32_MSR_MTRRCAP_VCNT_MASK      (0xff)

#define IA32_MSR_PAT                    0x277

#define IA32_MSR_MTRR_DEFTYPE           0x2FF
#define IA32_MSR_MTRR_DEFTYPE_E         (1 << 11)
#define IA32_MSR_MTRR_DEFTYPE_FE        (1 << 10)
#define IA32_MSR_MTRR_DEFTYPE_TYPE_MASK (0xff)

#define IA32_MSR_MTRR_FIX_64K_00000     0x250
#define IA32_MSR_MTRR_FIX_16K_80000     0x258
#define IA32_MSR_MTRR_FIX_16K_A0000     0x259
#define IA32_MSR_MTRR_FIX_4K_C0000      0x268
#define IA32_MSR_MTRR_FIX_4K_C8000      0x269
#define IA32_MSR_MTRR_FIX_4K_D0000      0x26a
#define IA32_MSR_MTRR_FIX_4K_D8000      0x26b
#define IA32_MSR_MTRR_FIX_4K_E0000      0x26c
#define IA32_MSR_MTRR_FIX_4K_E8000      0x26d
#define IA32_MSR_MTRR_FIX_4K_F0000      0x26e
#define IA32_MSR_MTRR_FIX_4K_F8000      0x26f
#define IA32_MSR_MTRR_FIX_COUNT         11

#define IA32_MSR_MTRR_PHYSBASE(n)       (0x200 + (n) * 2)
#define IA32_MSR_MTRR_PHYSMASK(n)       (0x201 + (n) * 2)
#define IA32_MSR_MTRR_PHYSMASK_V        (1 << 11)
