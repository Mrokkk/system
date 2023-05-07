#pragma once

#include <stdint.h>
#include <kernel/kernel.h>

// Reference: https://uefi.org/sites/default/files/resources/ACPI_6_2.pdf

struct gas;
struct fadt;
struct rsdt;
struct sdt_header;
struct rsdp_descriptor;

typedef struct gas gas_t;
typedef struct fadt fadt_t;
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

struct rsdp_descriptor
{
    char        signature[8];
    uint8_t     checksum;
    char        oemid[6];
    uint8_t     revision;
    uint32_t    rsdt_addr;
    // ACPI 2.0
    uint32_t    len;
    uint64_t    xsdt_addr;
    uint8_t     extended_checksum;
    uint8_t     reserved[3];
} PACKED;

struct rsdt
{
    sdt_header_t header;
    uint32_t table[];
} PACKED;

struct gas
{
    uint8_t     address_space;
    uint8_t     bit_width;
    uint8_t     bit_offset;
    uint8_t     access_size;
    uint64_t    address;
} PACKED;

#define IAPC_BOOT_LEGACY    (1 << 0)
#define IAPC_BOOT_8042      (1 << 1)
#define IAPC_BOOT_NO_VGA    (1 << 2)

struct fadt
{
    sdt_header_t header;
    uint32_t    firmware_ctrl;
    uint32_t    dsdt;
    uint8_t     reserved;
    uint8_t     preferred_pm_profile;
    uint16_t    sci_int;
    uint32_t    smi_cmd;
    uint8_t     acpi_enable;
    uint8_t     acpi_disable;
    uint8_t     s4bios_req;
    uint8_t     pstate_cnt;
    uint32_t    pm1a_evt_blk;
    uint32_t    pm1b_evt_blk;
    uint32_t    pm1a_cnt_blk;
    uint32_t    pm1b_cnt_blk;
    uint32_t    pm2_cnt_blk;
    uint32_t    pm_tmr_blk;
    uint32_t    gpe0_blk;
    uint32_t    gpe1_blk;
    uint8_t     pm1_evt_len;
    uint8_t     pm1_cnt_len;
    uint8_t     pm2_cnt_len;
    uint8_t     pm_tmr_len;
    uint8_t     gpe0_blk_len;
    uint8_t     gpe1_blk_len;
    uint8_t     gpe1_base;
    uint8_t     cst_cnt;
    uint16_t    p_lvl2_lat;
    uint16_t    p_lvl3_lat;
    uint16_t    flush_size;
    uint16_t    flush_stride;
    uint8_t     duty_offset;
    uint8_t     duty_width;
    uint8_t     day_alrm;
    uint8_t     mon_alrm;
    uint8_t     century;

    // reserved in ACPI 1.0; used since ACPI 2.0
    uint16_t    iapc_boot_arch;

    uint8_t     reserved2;
    uint32_t    flags;

    gas_t       reset_req;

    uint8_t     reset_value;
    uint8_t     reserved3[3];

    // available since ACPI 2.0
    uint64_t    X_FirmwareControl;
    uint64_t    X_Dsdt;

    gas_t X_PM1aEventBlock;
    gas_t X_PM1bEventBlock;
    gas_t X_PM1aControlBlock;
    gas_t X_PM1bControlBlock;
    gas_t X_PM2ControlBlock;
    gas_t X_PMTimerBlock;
    gas_t X_GPE0Block;
    gas_t X_GPE1Block;
} PACKED;

void acpi_initialize(void);
void* acpi_find(char* signature);
