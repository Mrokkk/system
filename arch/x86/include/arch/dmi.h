#pragma once

#include <stdint.h>

struct dmi;
typedef struct dmi dmi_t;

struct dmi
{
    const char* manufacturer;
    const char* product;
    const char* version;
    const char* bios;
    const char* bios_version;
    struct
    {
        uint32_t isa:1;
        uint32_t eisa:1;
        uint32_t pci:1;
        uint32_t apm:1;
        uint32_t acpi:1;
    } support;
};

void dmi_read(void);

extern dmi_t dmi;
