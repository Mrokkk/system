#pragma once

#include <stdint.h>
#include <kernel/kernel.h>

// Reference: https://uefi.org/sites/default/files/resources/ACPI_6_2.pdf

typedef struct rsdt rsdt_t;
typedef struct sdt_header sdt_header_t;
typedef struct rsdp_descriptor rsdp_descriptor_t;

struct sdt_header
{
    char        signature[4];
    uint32_t    len;
    uint8_t     revision;
    uint8_t     checksum;
    char        oem_id[6];
    char        oem_table_id[8];
    uint32_t    oem_revision;
    uint32_t    creator_id;
    uint32_t    creator_revision;
} PACKED;

void acpi_initialize(void);
void acpi_finalize(void);
void* acpi_find(char* signature);

extern bool acpi_enabled;
