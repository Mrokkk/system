#pragma once

#include <arch/io.h>
#include <kernel/compiler.h>
#include <kernel/page_alloc.h>

// References:
// https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/serial-ata-ahci-spec-rev1-3-1.pdf
// https://wiki.osdev.org/AHCI
// https://stackoverflow.com/questions/11739979/ahci-driver-for-own-os

typedef struct ahci_port ahci_port_t;
typedef struct ahci_port_data ahci_port_data_t;

typedef struct ahci_hba ahci_hba_t;
typedef struct ahci_hba_port ahci_hba_port_t;

typedef struct ahci_received_fis ahci_received_fis_t;
typedef struct ahci_command_table ahci_command_table_t;
typedef struct ahci_command ahci_command_t;
typedef struct ahci_prdt_entry ahci_prdt_entry_t;

#define AHCI_DEV_NULL       0
#define AHCI_DEV_SATA       1
#define AHCI_DEV_SEMB       2
#define AHCI_DEV_PM         3
#define AHCI_DEV_SATAPI     4

#define AHCI_REG(name, val) \
    AHCI_##name = val

#define AHCI_VALUE(reg, name, mask, bit) \
    AHCI_##reg##_##name##_BIT = (bit), \
    AHCI_##reg##_##name##_MASK = ((mask) << (bit)), \
    AHCI_##reg##_##name = ((mask) << (bit)), \
    AHCI_##reg##_##name##_RAW_MASK = (mask)

#define AHCI_PORT_COUNT 32

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

    // Command Completion Coalescing Ports
    AHCI_REG(CCC_PORTS, 0x18),

    // HBA Capabilities Extended
    AHCI_REG(CAP2, 0x24),
        AHCI_VALUE(CAP2, BOH, 1, 0),    // BIOS/OS Handoff

    // BIOS/OS Handoff Control and Status
    AHCI_REG(BOHC, 0x28),
        AHCI_VALUE(BOHC, BB, 1, 4),     // BIOS Busy (BB)
        AHCI_VALUE(BOHC, OOC, 1, 3),    // OS Ownership Change
        AHCI_VALUE(BOHC, SOOE, 1, 2),   // SMI on OS Ownership Change Enable
        AHCI_VALUE(BOHC, OOS, 1, 1),    // OS Owned Semaphore
        AHCI_VALUE(BOHC, BOS, 1, 0),    // BIOS Owned Semaphore

    AHCI_REG(PxCLB, 0x00),          // Port x Command List Base Address
    AHCI_REG(PxCLBU, 0x04),         // Port x Command List Base Address Upper 32-Bits
    AHCI_REG(PxFB, 0x08),           // Port x FIS Base Address
    AHCI_REG(PxFBU, 0x0c),          // Port x FIS Base Address Upper 32-Bits

    // Port x Interrupt Status
    AHCI_REG(PxIS, 0x10),
        AHCI_VALUE(PxIS, CPDS, 1, 31),  // Port x Cold Port Detect Status
        AHCI_VALUE(PxIS, TFES, 1, 30),  // Port x Task File Error Status
        AHCI_VALUE(PxIS, HBFS, 1, 29),  // Port x Host Bus Fatal Error Status
        AHCI_VALUE(PxIS, HBDS, 1, 28),  // Port x Host Bus Data Error Status
        AHCI_VALUE(PxIS, IFS, 1, 27),   // Port x Interface Fatal Error Status
        AHCI_VALUE(PxIS, INFS, 1, 26),  // Port x Interface Non-fatal Error Status
        AHCI_VALUE(PxIS, OFS, 1, 24),   // Port x Overflow Status
        AHCI_VALUE(PxIS, IPMS, 1, 23),  // Port x Incorrect Port Multiplier Status
        AHCI_VALUE(PxIS, PRCS, 1, 22),  // Port x PhyRdy Change Status
        AHCI_VALUE(PxIS, DMPS, 1, 7),   // Port x Device Mechanical Presence Status
        AHCI_VALUE(PxIS, PCS, 1, 6),    // Port x Port Connect Change Status
        AHCI_VALUE(PxIS, DPS, 1, 5),    // Port x Descriptor Processed
        AHCI_VALUE(PxIS, UFS, 1, 4),    // Port x Unknown FIS Interrupt
        AHCI_VALUE(PxIS, SDBS, 1, 3),   // Port x Set Device Bits Interrupt
        AHCI_VALUE(PxIS, DSS, 1, 2),    // Port x DMA Setup FIS Interrupt
        AHCI_VALUE(PxIS, PSS, 1, 1),    // Port x PIO Setup FIS Interrupt
        AHCI_VALUE(PxIS, DHRS, 1, 0),   // Port x Device to Host Register FIS Interrupt

    AHCI_REG(PxIE, 0x14),           // Port x Interrupt Enable

    // Port x Command and Status
    AHCI_REG(PxCMD, 0x18),
        AHCI_VALUE(PxCMD, ASP, 1, 27),  // Aggressive Slumber / Partial
        AHCI_VALUE(PxCMD, ATAPI, 1, 24),// Device is ATAPI
        AHCI_VALUE(PxCMD, ESP, 1, 21),  // External SATA Port
        AHCI_VALUE(PxCMD, CPD, 1, 20),  // Cold Presence Detection
        AHCI_VALUE(PxCMD, MPSP, 1, 19), // Mechanical Presence Switch Attached to Port
        AHCI_VALUE(PxCMD, HPCP, 1, 18), // Hot Plug Capable Port
        AHCI_VALUE(PxCMD, PMA, 1, 17),  // Port Multiplier Attached
        AHCI_VALUE(PxCMD, CPS, 1, 16),  // Cold Presence State
        AHCI_VALUE(PxCMD, CR, 1, 15),   // Command Running
        AHCI_VALUE(PxCMD, FR, 1, 14),   // FIS Receive Running
        AHCI_VALUE(PxCMD, MPSS, 1, 13), // Mechanical Presence Switch State
        AHCI_VALUE(PxCMD, CCS, 0x1f, 8),// Current Command Slot
        AHCI_VALUE(PxCMD, FRE, 1, 4),   // FIS Receive Enable
        AHCI_VALUE(PxCMD, CLO, 1, 3),   // Command List Override
        AHCI_VALUE(PxCMD, POD, 1, 2),   // Power On Device
        AHCI_VALUE(PxCMD, SUD, 1, 1),   // Spin-Up Device
        AHCI_VALUE(PxCMD, ST, 1, 0),    // Start

    // Port x Task File Data
    AHCI_REG(PxTFD, 0x20),
        AHCI_VALUE(PxTFD, ERR, 0xff, 8),    // Error
        AHCI_VALUE(PxTFD, STS, 0xff, 0),    // Status
            AHCI_VALUE(PxTFD, STS_BSY, 1, 7),   // Indicates the interface is busy
            AHCI_VALUE(PxTFD, STS_CS2, 0x7, 4), // Command specific 2
            AHCI_VALUE(PxTFD, STS_DRQ, 1, 3),   // Indicates a data transfer is requested
            AHCI_VALUE(PxTFD, STS_CS1, 0x3, 1), // Command specific 1
            AHCI_VALUE(PxTFD, STS_ERR, 1, 0),   // Indicates an error during the transfer

    AHCI_REG(PxSIG, 0x24),
        AHCI_VALUE(PxSIG, ATA, 0x00000101, 0),      // SATA drive
        AHCI_VALUE(PxSIG, ATAPI, 0xeb140101, 0),    // SATAPI drive
        AHCI_VALUE(PxSIG, SEMB, 0xc33c0101, 0),     // Enclosure management bridge
        AHCI_VALUE(PxSIG, PM, 0x96690101, 0),       // Port multiplier

    // Port x Serial ATA Status (SCR0: SStatus)
    AHCI_REG(PxSSTS, 0x28),
        AHCI_VALUE(PxSSTS, IPM, 0xf, 8),        // Interface Power Management
        AHCI_VALUE(PxSSTS, IPM_ACTIVE, 1, 8),   // Interface in active state
        AHCI_VALUE(PxSSTS, SPD, 0xf, 4),        // Current Interface Speed
        AHCI_VALUE(PxSSTS, DET, 0xf, 0),        // Device Detection
        AHCI_VALUE(PxSSTS, DET_PRESENT, 3, 0),  // Device Detection

    AHCI_REG(PxSCTL, 0x2c),     // Port x Serial ATA Control (SCR2: SControl)

    // Port x Serial ATA Error (SCR1: SError)
    AHCI_REG(PxSERR, 0x30),
        AHCI_VALUE(PxSERR, DIAG_X, 1, 26),  // Exchanged
        AHCI_VALUE(PxSERR, DIAG_F, 1, 25),  // Unknown FIS Type
        AHCI_VALUE(PxSERR, DIAG_T, 1, 24),  // Transport state transition error
        AHCI_VALUE(PxSERR, DIAG_S, 1, 23),  // Link Sequence Error
        AHCI_VALUE(PxSERR, DIAG_H, 1, 22),  // Handshake Error
        AHCI_VALUE(PxSERR, DIAG_C, 1, 21),  // CRC Error
        AHCI_VALUE(PxSERR, DIAG_D, 1, 20),  // Disparity Error
        AHCI_VALUE(PxSERR, DIAG_B, 1, 19),  // 10B to 8B Decode Error
        AHCI_VALUE(PxSERR, DIAG_W, 1, 18),  // Comm Wake
        AHCI_VALUE(PxSERR, DIAG_I, 1, 17),  // Phy Internal Error
        AHCI_VALUE(PxSERR, DIAG_N, 1, 16),  // PhyRdy Change
        AHCI_VALUE(PxSERR, ERR_E, 1, 11),   // Internal Error
        AHCI_VALUE(PxSERR, ERR_P, 1, 10),   // Protocol Error
        AHCI_VALUE(PxSERR, ERR_C, 1, 9),    // Persistent Communication or Data Integrity Error
        AHCI_VALUE(PxSERR, ERR_T, 1, 8),    // Transient Data Integrity Error
        AHCI_VALUE(PxSERR, ERR_M, 1, 1),    // Recovered Communications Error
        AHCI_VALUE(PxSERR, ERR_I, 1, 0),    // Recovered Data Integrity Error

    AHCI_REG(PxSACT, 0x34),     // Port x Serial ATA Active (SCR3: SActive)
    AHCI_REG(PxCI, 0x38),       // Port x Command Issue
    AHCI_REG(PxSNTF, 0x3c),     // Port x Serial ATA Notification (SCR4: SNotification)
    AHCI_REG(PxFBS, 0x40),      // Port x FIS-based Switching Control
    AHCI_REG(PxDEVSLP, 0x44),   // Port x Device Sleep
    AHCI_REG(PxVS, 0x70),       // Port x Vendor Specific
};

struct ahci_command
{
    // DW0
    io8  cfl:5;     // Command FIS length in DWORDS, 2 ~ 16
    io8  a:1;       // ATAPI
    io8  w:1;       // Write, 1: H2D, 0: D2H
    io8  p:1;       // Prefetchable

    io8  r:1;       // Reset
    io8  b:1;       // BIST
    io8  c:1;       // Clear busy upon R_OK
    io8  rsv0:1;    // Reserved
    io8  pmp:4;     // Port multiplier port
    io16 prdtl;     // Physical region descriptor table length in entries

    // DW1
    io32 prdbc;         // Physical region descriptor byte count transferred

    // DW2, 3
    io32 ctba;      // Command table descriptor base address
    io32 ctbau;     // Command table descriptor base address upper 32 bits

    // DW4 - 7
    io32 rsv1[4];   // Reserved
} PACKED;

struct ahci_prdt_entry
{
    io32 dba;       // Data base address
    io32 dbau;      // Data base address upper 32 bits
    io32 rsv0;      // Reserved

    // DW3
    io32 dbc:22;    // Byte count, 4M max
    io32 rsv1:9;    // Reserved
    io32 i:1;       // Interrupt on completion
} PACKED;

struct ahci_command_table
{
    // 0x00
    io8  cfis[64];  // Command FIS

    // 0x40
    io8  acmd[16];  // ATAPI command, 12 or 16 bytes

    // 0x50
    io8  rsv[48];   // Reserved

    // 0x80
    ahci_prdt_entry_t prdt_entry[0]; // Physical region descriptor table entries, 0 ~ 65535
} PACKED;

struct ahci_received_fis
{
    // 0x00
    io8  dsfis[28]; // DMA Setup FIS
    io8  pad0[4];

    // 0x20
    io8  psfis[20]; // PIO Setup FIS
    io8  pad1[12];

    // 0x40
    io8  rfis[20];  // Register – Device to Host FIS
    io8  pad2[4];

    // 0x58
    io64 sdbfis;    // Set Device Bit FIS

    // 0x60
    io8  ufis[64];

    // 0xA0
    io8  rsv[0x100 - 0xa0];
} PACKED;

struct ahci_hba_port
{
    io32 clb;       // 0x00, command list base address, 1K-byte aligned
    io32 clbu;      // 0x04, command list base address upper 32 bits
    io32 fb;        // 0x08, FIS base address, 256-byte aligned
    io32 fbu;       // 0x0C, FIS base address upper 32 bits
    io32 is;        // 0x10, interrupt status
    io32 ie;        // 0x14, interrupt enable
    //union cmd
    //{
        //io32 value;
        //struct
        //{
            //io32 st:1;
            //io32 sud:1;
            //io32 pod:1;
            //io32 clo:1;
            //io32 fre:1;
            //io32 rsv:3;
            //io32 ccs:5;
            //io32 mpss:1;
            //io32 fr:1;
            //io32 cr:1;
            //io32 cps:1;
            //io32 pma:1;
            //io32 hpcp:1;
            //io32 mpsp:1;
            //io32 cpd:1;
            //io32 esp:1;
            //io32 fbscp:1;
            //io32 apste:1;
            //io32 atapi:1;
            //io32 dlae:1;
            //io32 alpe:1;
            //io32 asp:1;
            //io32 icc:4;
        //};
    //} cmd;
    io32 cmd;       // 0x18, command and status
    io32 rsv0;      // 0x1C, Reserved
    io32 tfd;       // 0x20, task file data
    io32 sig;       // 0x24, signature
    io32 ssts;      // 0x28, SATA status (SCR0:SStatus)
    io32 sctl;      // 0x2C, SATA control (SCR2:SControl)
    io32 serr;      // 0x30, SATA error (SCR1:SError)
    io32 sact;      // 0x34, SATA active (SCR3:SActive)
    io32 ci;        // 0x38, command issue
    io32 sntf;      // 0x3C, SATA notification (SCR4:SNotification)
    io32 fbs;       // 0x40, FIS-based switch control
    io32 rsv1[11];  // 0x44 ~ 0x6F, Reserved
    io32 vendor[4]; // 0x70 ~ 0x7F, vendor specific
};

struct ahci_hba
{
    // 0x00, Host capability
    union cap
    {
        io32 value;
        struct
        {
            io32 np:5;
            io32 sxs:1;
            io32 ems:1;
            io32 cccs:1;
            io32 ncs:5;
            io32 psc:1;
            io32 ssc:1;
            io32 pmd:1;
            io32 fbss:1;
            io32 spm:1;
            io32 sam:1;
            io32 rsv:1;
            io32 iss:4;
            io32 sclo:1;
            io32 sal:1;
            io32 salp:1;
            io32 sss:1;
            io32 smps:1;
            io32 ssntf:1;
            io32 sncq:1;
            io32 s64a:1;
        };
    } cap;

    // 0x04, Global host control
    union ghc
    {
        io32 value;
        struct
        {
            io32 hr:1;
            io32 ie:1;
            io32 mrsm:1;
            io32 rsv:28;
            io32 ae:1;
        };
    } ghc;

    io32 is;        // 0x08, Interrupt status
    io32 pi;        // 0x0C, Port implemented

    // 0x10, Version
    union vs
    {
        io32 value;
        struct
        {
            io16 mnr;
            io16 mjr;
        };
    } vs;

    // 0x14, Command completion coalescing control
    io32 ccc_ctl;

    // 0x18, Command completion coalescing ports
    io32 ccc_pts;

    // 0x1C, Enclosure management location
    io32 em_loc;

    // 0x20, Enclosure management control
    io32 em_ctl;

    // 0x24, Host capabilities extended
    io32 cap2;

    // 0x28, BIOS/OS handoff control and status
    io32 bohc;

    // 0x2C - 0x9F, Reserved
    io8 rsv[0xa0 - 0x2c];

    // 0xA0 - 0xFF, Vendor specific registers
    io8 vendor[0x100 - 0xa0];

    // 0x100 - 0x10FF, Port control registers
    ahci_hba_port_t ports[AHCI_PORT_COUNT];
};

struct ahci_port
{
    uint8_t             id;
    uint32_t            signature;
    ahci_hba_port_t*    regs;
    page_t*             data_pages;
    ahci_port_data_t*   data;
};

#define CMDLIST_COUNT 32
#define PRDT_COUNT 4

struct ahci_port_data
{
    ahci_received_fis_t     fis;
    uint8_t                 pad[1024 - sizeof(ahci_received_fis_t)];
    ahci_command_t          cmdlist[CMDLIST_COUNT];
    ahci_command_table_t    cmdtable;
    ahci_prdt_entry_t       prdt[PRDT_COUNT];
};

void ahci_dump(ahci_hba_t* ahci);
void ahci_port_dump(ahci_hba_port_t* ahci);
void ahci_port_error_print(ahci_port_t* port);
