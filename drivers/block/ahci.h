#pragma once

#include <arch/io.h>
#include "ata.h"

// References:
// https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/serial-ata-ahci-spec-rev1-3-1.pdf
// https://wiki.osdev.org/AHCI

struct ahci_port;
typedef struct ahci_port ahci_port_t;

#define AHCI_DEV_NULL       0
#define AHCI_DEV_SATA       1
#define AHCI_DEV_SEMB       2
#define AHCI_DEV_PM         3
#define AHCI_DEV_SATAPI     4

#define SATA_SIG_ATA        0x00000101 // SATA drive
#define SATA_SIG_ATAPI      0xeb140101 // SATAPI drive
#define SATA_SIG_SEMB       0xc33c0101 // Enclosure management bridge
#define SATA_SIG_PM         0x96690101 // Port multiplier

struct hba;
struct port;

typedef struct hba hba_t;
typedef struct port port_t;

#define AHCI_REG(name, val) \
    AHCI_##name = val

#define AHCI_VALUE(reg, name, mask, bit) \
    AHCI_##reg##_##name##_BIT = (bit), \
    AHCI_##reg##_##name##_MASK = ((mask) << (bit)), \
    AHCI_##reg##_##name = ((mask) << (bit))

enum
{
    // HBA Capabilities
    AHCI_REG(CAP, 0x00),
    AHCI_VALUE(CAP, S64A, 1, 31),   // Supports 64-bit Addressing
    AHCI_VALUE(CAP, SNCQ, 1, 30),   // Supports Native Command Queuing
    AHCI_VALUE(CAP, SSNTF, 1, 29),  // Supports SNotification Register
    AHCI_VALUE(CAP, SMPS, 1, 28),   // Supports Mechanical Presence Switch
    AHCI_VALUE(CAP, SSS, 1, 27),    // Supports Staggered Spin-up
    AHCI_VALUE(CAP, SALP, 1, 26),   // Supports Aggressive Link Power Management
    AHCI_VALUE(CAP, SAL, 1, 25),    // Supports Activity LED
    AHCI_VALUE(CAP, SCLO, 1, 24),   // Supports Command List Override
    AHCI_VALUE(CAP, ISS, 0xf, 20),  // Interface Speed Support
    AHCI_VALUE(CAP, ISS_1_5GBPS, 0x1, 20),  // 1.5 Gbps
    AHCI_VALUE(CAP, ISS_3GBPS, 0x2, 20),    // 3 Gbps
    AHCI_VALUE(CAP, ISS_6GBPS, 0x3, 20),    // 6 Gbps
    AHCI_VALUE(CAP, SAM, 1, 18),    // Supports AHCI mode only
    AHCI_VALUE(CAP, SPM, 1, 17),    // Supports Port Multiplier
    AHCI_VALUE(CAP, FBSS, 1, 16),   // FIS-based Switching Supported
    AHCI_VALUE(CAP, PMD, 1, 15),    // PIO Multiple DRQ Block
    AHCI_VALUE(CAP, SSC, 1, 14),    // Slumber State Capable
    AHCI_VALUE(CAP, PSC, 1, 13),    // Partial State Capable
    AHCI_VALUE(CAP, NCS, 0x1f, 8),  // Number of Command Slots
    AHCI_VALUE(CAP, CCCS, 1, 7),    // Command Completion Coalescing Supported
    AHCI_VALUE(CAP, EMS, 1, 6),     // Enclosure Management Supported
    AHCI_VALUE(CAP, SXS, 1, 5),     // Supports External SATA
    AHCI_VALUE(CAP, NP, 0x1f, 0),   // Number of Ports

    // Global HBA Control
    AHCI_REG(GHC, 0x04),
    AHCI_VALUE(GHC, AE, 1, 31),     // AHCI Enable
    AHCI_VALUE(GHC, MRSM, 1, 2),    // MSI Revert to Single Message
    AHCI_VALUE(GHC, IE, 1, 1),      // Interrupt Enable
    AHCI_VALUE(GHC, HR, 1, 0),      // HBA Reset

    // Interrupt Status Register
    AHCI_REG(IS, 0x06),

    // Ports Implemented
    AHCI_REG(PI, 0x0c),

    // AHCI Version
    AHCI_REG(VS, 0x10),

    // Command Completion Coalescing Control
    AHCI_REG(CCC_CTL, 0x14),
    AHCI_CCC_CTL_TV = (0xffff << 16),   // Timeout Value
    AHCI_CCC_CTL_CC = (0xff << 8),      // Command Completions
    AHCI_CCC_CTL_INT = (0x1f << 3),     // Interrupt
    AHCI_CCC_CTL_EN = (1 << 0),         // Enable

    // Command Completion Coalescing Ports
    AHCI_REG(CCC_PORTS, 0x18),

    // BIOS/OS Handoff Control and Status
    AHCI_REG(BOHC, 0x28),
    AHCI_VALUE(BOHC, BB, 1, 4),     // BIOS Busy (BB)
    AHCI_VALUE(BOHC, OOC, 1, 3),    // OS Ownership Change
    AHCI_VALUE(BOHC, SOOE, 1, 2),   // SMI on OS Ownership Change Enable
    AHCI_VALUE(BOHC, OOS, 1, 1),    // OS Owned Semaphore
    AHCI_VALUE(BOHC, BOS, 1, 0),    // BIOS Owned Semaphore

    AHCI_PxCLB      = 0x00, // Port x Command List Base Address
    AHCI_PxCLBU     = 0x04, // Port x Command List Base Address Upper 32-Bits
    AHCI_PxFB       = 0x08, // Port x FIS Base Address
    AHCI_PxFBU      = 0x0c, // Port x FIS Base Address Upper 32-Bits
    AHCI_PxIS       = 0x10, // Port x Interrupt Status
    AHCI_PxIE       = 0x14, // Port x Interrupt Enable
    AHCI_PxCMD      = 0x18, // Port x Command and Status
    AHCI_VALUE(PxCMD, FRE, 1, 4), // FIS Receive Enable
    AHCI_PxTFD      = 0x20, // Port x Task File Data
    AHCI_PxSIG      = 0x24, // Port x Signature
    AHCI_PxSSTS     = 0x28, // Port x Serial ATA Status (SCR0: SStatus)
    AHCI_VALUE(PxSSTS, IPM, 0xf, 8),    // Interface Power Management
    AHCI_VALUE(PxSSTS, IPM_ACTIVE, 1, 8),   // Interface in active state
    AHCI_VALUE(PxSSTS, DET, 0xf, 0),    // Device Detection
    AHCI_VALUE(PxSSTS, DET_PRESENT, 3, 0),  // Device Detection

    AHCI_PxSCTL     = 0x2c, // Port x Serial ATA Control (SCR2: SControl)
    AHCI_PxSERR     = 0x30, // Port x Serial ATA Error (SCR1: SError)
    AHCI_PxSACT     = 0x34, // Port x Serial ATA Active (SCR3: SActive)
    AHCI_PxCI       = 0x38, // Port x Command Issue
    AHCI_PxSNTF     = 0x3c, // Port x Serial ATA Notification (SCR4: SNotification)
    AHCI_PxFBS      = 0x40, // Port x FIS-based Switching Control
    AHCI_PxDEVSLP   = 0x44, // Port x Device Sleep
    AHCI_PxVS       = 0x70, // Port x Vendor Specific

    FIS_TYPE_REG_H2D    = 0x27, // Register FIS - host to device
    FIS_TYPE_REG_D2H    = 0x34, // Register FIS - device to host
    FIS_TYPE_DMA_ACT    = 0x39, // DMA activate FIS - device to host
    FIS_TYPE_DMA_SETUP  = 0x41, // DMA setup FIS - bidirectional
    FIS_TYPE_DATA       = 0x46, // Data FIS - bidirectional
    FIS_TYPE_BIST       = 0x58, // BIST activate FIS - bidirectional
    FIS_TYPE_PIO_SETUP  = 0x5F, // PIO setup FIS - device to host
    FIS_TYPE_DEV_BITS   = 0xA1, // Set device bits FIS - device to host
};

typedef struct tagFIS_REG_H2D
{
    // DWORD 0
    uint8_t  fis_type;  // FIS_TYPE_REG_H2D
    uint8_t  pmport:4;  // Port multiplier
    uint8_t  rsv0:3;    // Reserved
    uint8_t  c:1;       // 1: Command, 0: Control
    uint8_t  command;   // Command register
    uint8_t  featurel;  // Feature register, 7:0

    // DWORD 1
    uint8_t  lba0;      // LBA low register, 7:0
    uint8_t  lba1;      // LBA mid register, 15:8
    uint8_t  lba2;      // LBA high register, 23:16
    uint8_t  device;    // Device register

    // DWORD 2
    uint8_t  lba3;      // LBA register, 31:24
    uint8_t  lba4;      // LBA register, 39:32
    uint8_t  lba5;      // LBA register, 47:40
    uint8_t  featureh;  // Feature register, 15:8

    // DWORD 3
    uint8_t  countl;    // Count register, 7:0
    uint8_t  counth;    // Count register, 15:8
    uint8_t  icc;       // Isochronous command completion
    uint8_t  control;   // Control register

    // DWORD 4
    uint8_t  rsv1[4];   // Reserved
} FIS_REG_H2D;

typedef struct tagFIS_REG_D2H
{
    // DWORD 0
    uint8_t  fis_type;  // FIS_TYPE_REG_D2H
    uint8_t  pmport:4;  // Port multiplier
    uint8_t  rsv0:2;    // Reserved
    uint8_t  i:1;       // Interrupt bit
    uint8_t  rsv1:1;    // Reserved
    uint8_t  status;    // Status register
    uint8_t  error;     // Error register

    // DWORD 1
    uint8_t  lba0;      // LBA low register, 7:0
    uint8_t  lba1;      // LBA mid register, 15:8
    uint8_t  lba2;      // LBA high register, 23:16
    uint8_t  device;    // Device register

    // DWORD 2
    uint8_t  lba3;      // LBA register, 31:24
    uint8_t  lba4;      // LBA register, 39:32
    uint8_t  lba5;      // LBA register, 47:40
    uint8_t  rsv2;      // Reserved

    // DWORD 3
    uint8_t  countl;    // Count register, 7:0
    uint8_t  counth;    // Count register, 15:8
    uint8_t  rsv3[2];   // Reserved

    // DWORD 4
    uint8_t  rsv4[4];   // Reserved
} FIS_REG_D2H;

typedef struct tagFIS_DATA
{
    // DWORD 0
    uint8_t  fis_type;  // FIS_TYPE_DATA
    uint8_t  pmport:4;  // Port multiplier
    uint8_t  rsv0:4;    // Reserved
    uint8_t  rsv1[2];   // Reserved

    // DWORD 1 ~ N
    uint32_t data[1];   // Payload
} FIS_DATA;

typedef struct tagFIS_PIO_SETUP
{
    // DWORD 0
    uint8_t  fis_type;  // FIS_TYPE_PIO_SETUP
    uint8_t  pmport:4;  // Port multiplier
    uint8_t  rsv0:1;    // Reserved
    uint8_t  d:1;       // Data transfer direction, 1 - device to host
    uint8_t  i:1;       // Interrupt bit
    uint8_t  rsv1:1;
    uint8_t  status;    // Status register
    uint8_t  error;     // Error register

    // DWORD 1
    uint8_t  lba0;      // LBA low register, 7:0
    uint8_t  lba1;      // LBA mid register, 15:8
    uint8_t  lba2;      // LBA high register, 23:16
    uint8_t  device;    // Device register

    // DWORD 2
    uint8_t  lba3;      // LBA register, 31:24
    uint8_t  lba4;      // LBA register, 39:32
    uint8_t  lba5;      // LBA register, 47:40
    uint8_t  rsv2;      // Reserved

    // DWORD 3
    uint8_t  countl;    // Count register, 7:0
    uint8_t  counth;    // Count register, 15:8
    uint8_t  rsv3;      // Reserved
    uint8_t  e_status;  // New value of status register

    // DWORD 4
    uint16_t tc;        // Transfer count
    uint8_t  rsv4[2];   // Reserved
} FIS_PIO_SETUP;

typedef struct tagFIS_DMA_SETUP
{
    // DWORD 0
    uint8_t  fis_type;  // FIS_TYPE_DMA_SETUP
    uint8_t  pmport:4;  // Port multiplier
    uint8_t  rsv0:1;    // Reserved
    uint8_t  d:1;       // Data transfer direction, 1 - device to host
    uint8_t  i:1;       // Interrupt bit
    uint8_t  a:1;       // Auto-activate. Specifies if DMA Activate FIS is needed
    uint8_t  rsved[2];  // Reserved

    //DWORD 1&2
    uint64_t dma_bufid; // DMA Buffer Identifier. Used to Identify DMA buffer in host memory.
                        // SATA Spec says host specific and not in Spec. Trying AHCI spec might work.

    //DWORD 3
    uint32_t rsvd;

    //DWORD 4
    uint32_t dma_bufoff;//Byte offset into buffer. First 2 bits must be 0

    //DWORD 5
    uint32_t count;     //Number of bytes to transfer. Bit 0 must be 0

    //DWORD 6
    uint32_t resvd;
} FIS_DMA_SETUP;

typedef struct tagHBA_CMD_HEADER
{
    // DW0
    uint8_t  cfl:5;     // Command FIS length in DWORDS, 2 ~ 16
    uint8_t  a:1;       // ATAPI
    uint8_t  w:1;       // Write, 1: H2D, 0: D2H
    uint8_t  p:1;       // Prefetchable

    uint8_t  r:1;       // Reset
    uint8_t  b:1;       // BIST
    uint8_t  c:1;       // Clear busy upon R_OK
    uint8_t  rsv0:1;    // Reserved
    uint8_t  pmp:4;     // Port multiplier port

    uint16_t prdtl;     // Physical region descriptor table length in entries

    // DW1
    io32 prdbc;         // Physical region descriptor byte count transferred

    // DW2, 3
    uint32_t ctba;      // Command table descriptor base address
    uint32_t ctbau;     // Command table descriptor base address upper 32 bits

    // DW4 - 7
    uint32_t rsv1[4];   // Reserved
} HBA_CMD_HEADER;

typedef struct tagHBA_PRDT_ENTRY
{
    uint32_t dba;       // Data base address
    uint32_t dbau;      // Data base address upper 32 bits
    uint32_t rsv0;      // Reserved

    // DW3
    uint32_t dbc:22;    // Byte count, 4M max
    uint32_t rsv1:9;    // Reserved
    uint32_t i:1;       // Interrupt on completion
} HBA_PRDT_ENTRY;

typedef struct tagHBA_CMD_TBL
{
    // 0x00
    uint8_t  cfis[64];  // Command FIS

    // 0x40
    uint8_t  acmd[16];  // ATAPI command, 12 or 16 bytes

    // 0x50
    uint8_t  rsv[48];   // Reserved

    // 0x80
    HBA_PRDT_ENTRY prdt_entry[]; // Physical region descriptor table entries, 0 ~ 65535
} HBA_CMD_TBL;


typedef volatile struct tagHBA_FIS
{
    // 0x00
    FIS_DMA_SETUP dsfis;    // DMA Setup FIS
    uint8_t     pad0[4];

    // 0x20
    FIS_PIO_SETUP psfis;    // PIO Setup FIS
    uint8_t     pad1[12];

    // 0x40
    FIS_REG_D2H rfis;       // Register – Device to Host FIS
    uint8_t     pad2[4];

    // 0x58
    uint64_t    sdbfis;     // Set Device Bit FIS

    // 0x60
    uint8_t     ufis[64];

    // 0xA0
    uint8_t     rsv[0x100 - 0xa0];
} HBA_FIS;

struct ahci_port
{
    uint32_t base_reg;
    HBA_CMD_HEADER* cmdlist;
    HBA_CMD_TBL* cmdtable;
    HBA_FIS* fis;
};
