#pragma once

#include <arch/descriptor.h>

#define KERNEL_CS   0x8
#define KERNEL_DS   0x10
#define USER_CS     0x1b
#define USER_DS     0x23
#define APM_CS      descriptor_selector(APM_CODE_ENTRY, 0)
#define APM_CS_16   descriptor_selector(APM_CODE_16_ENTRY, 0)
#define APM_DS      descriptor_selector(APM_DATA_ENTRY, 0)
