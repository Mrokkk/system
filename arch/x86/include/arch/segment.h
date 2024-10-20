#pragma once

#include <arch/descriptor.h>

#define KERNEL_CS    0x8
#define KERNEL_DS    0x10
#define USER_CS      0x1b
#define USER_DS      0x23
#define USER_TLS     descriptor_selector(TLS_ENTRY, 3)
#define APM_CS       descriptor_selector(APM_CODE_ENTRY, 0)
#define APM_CS_16    descriptor_selector(APM_CODE_16_ENTRY, 0)
#define APM_DS       descriptor_selector(APM_DATA_ENTRY, 0)
#define KERNEL_16_CS descriptor_selector(KERNEL_16_CODE, 0)
#define KERNEL_16_DS descriptor_selector(KERNEL_16_DATA, 0)

#ifdef __x86_64__
#define KERNEL_32_CS descriptor_selector(KERNEL_32_CODE, 0)
#endif
