#pragma once

#include <arch/io.h>

// References:
// https://sata-io.org/system/files/specifications/SerialATA_Revision_3_1_Gold.pdf

typedef struct sata_fis_h2d FIS_REG_H2D;
typedef struct sata_fis_d2h FIS_REG_D2H;
typedef struct sata_fis_pio_setup FIS_PIO_SETUP;
typedef struct sata_fis_dma_setup FIS_DMA_SETUP;

enum
{
    FIS_TYPE_REG_H2D    = 0x27, // Register FIS - host to device
    FIS_TYPE_REG_D2H    = 0x34, // Register FIS - device to host
    FIS_TYPE_DMA_ACT    = 0x39, // DMA activate FIS - device to host
    FIS_TYPE_DMA_SETUP  = 0x41, // DMA setup FIS - bidirectional
    FIS_TYPE_DATA       = 0x46, // Data FIS - bidirectional
    FIS_TYPE_BIST       = 0x58, // BIST activate FIS - bidirectional
    FIS_TYPE_PIO_SETUP  = 0x5F, // PIO setup FIS - device to host
    FIS_TYPE_DEV_BITS   = 0xA1, // Set device bits FIS - device to host
};

struct sata_fis_h2d
{
    // DWORD 0
    io8 fis_type;  // FIS_TYPE_REG_H2D
    io8 pmport:4;  // Port multiplier
    io8 rsv0:3;    // Reserved
    io8 c:1;       // 1: Command, 0: Control
    io8 command;   // Command register
    io8 featurel;  // Feature register, 7:0

    // DWORD 1
    io8 lba0;      // LBA low register, 7:0
    io8 lba1;      // LBA mid register, 15:8
    io8 lba2;      // LBA high register, 23:16
    io8 device;    // Device register

    // DWORD 2
    io8 lba3;      // LBA register, 31:24
    io8 lba4;      // LBA register, 39:32
    io8 lba5;      // LBA register, 47:40
    io8 featureh;  // Feature register, 15:8

    // DWORD 3
    io8 countl;    // Count register, 7:0
    io8 counth;    // Count register, 15:8
    io8 icc;       // Isochronous command completion
    io8 control;   // Control register

    // DWORD 4
    io8 rsv1[4];   // Reserved
} PACKED;

struct sata_fis_d2h
{
    // DWORD 0
    io8  fis_type;  // FIS_TYPE_REG_D2H
    io8  pmport:4;  // Port multiplier
    io8  rsv0:2;    // Reserved
    io8  i:1;       // Interrupt bit
    io8  rsv1:1;    // Reserved
    io8  status;    // Status register
    io8  error;     // Error register

    // DWORD 1
    io8  lba0;      // LBA low register, 7:0
    io8  lba1;      // LBA mid register, 15:8
    io8  lba2;      // LBA high register, 23:16
    io8  device;    // Device register

    // DWORD 2
    io8  lba3;      // LBA register, 31:24
    io8  lba4;      // LBA register, 39:32
    io8  lba5;      // LBA register, 47:40
    io8  rsv2;      // Reserved

    // DWORD 3
    io8  countl;    // Count register, 7:0
    io8  counth;    // Count register, 15:8
    io8  rsv3[2];   // Reserved

    // DWORD 4
    io8  rsv4[4];   // Reserved
} PACKED;

struct sata_fis_pio_setup
{
    // DWORD 0
    io8  fis_type;  // FIS_TYPE_PIO_SETUP
    io8  pmport:4;  // Port multiplier
    io8  rsv0:1;    // Reserved
    io8  d:1;       // Data transfer direction, 1 - device to host
    io8  i:1;       // Interrupt bit
    io8  rsv1:1;
    io8  status;    // Status register
    io8  error;     // Error register

    // DWORD 1
    io8  lba0;      // LBA low register, 7:0
    io8  lba1;      // LBA mid register, 15:8
    io8  lba2;      // LBA high register, 23:16
    io8  device;    // Device register

    // DWORD 2
    io8  lba3;      // LBA register, 31:24
    io8  lba4;      // LBA register, 39:32
    io8  lba5;      // LBA register, 47:40
    io8  rsv2;      // Reserved

    // DWORD 3
    io8  countl;    // Count register, 7:0
    io8  counth;    // Count register, 15:8
    io8  rsv3;      // Reserved
    io8  e_status;  // New value of status register

    // DWORD 4
    io16 tc;        // Transfer count
    io8  rsv4[2];   // Reserved
} PACKED;

struct sata_fis_dma_setup
{
    // DWORD 0
    io8  fis_type;  // FIS_TYPE_DMA_SETUP
    io8  pmport:4;  // Port multiplier
    io8  rsv0:1;    // Reserved
    io8  d:1;       // Data transfer direction, 1 - device to host
    io8  i:1;       // Interrupt bit
    io8  a:1;       // Auto-activate. Specifies if DMA Activate FIS is needed
    io8  rsved[2];  // Reserved

    //DWORD 1&2
    io64 dma_bufid;     // DMA Buffer Identifier. Used to Identify DMA buffer in host memory.
                        // SATA Spec says host specific and not in Spec. Trying AHCI spec might work.

    //DWORD 3
    io32 rsvd;

    //DWORD 4
    io32 dma_bufoff;    //Byte offset into buffer. First 2 bits must be 0

    //DWORD 5
    io32 count;     //Number of bytes to transfer. Bit 0 must be 0

    //DWORD 6
    io32 resvd;
} PACKED;
