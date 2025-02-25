#pragma once

#include <stdint.h>
#include <kernel/byteorder.h>

// References:
// https://www.seagate.com/files/staticfiles/support/docs/manual/Interface%20manuals/100293068j.pdf

typedef struct scsi_packet scsi_packet_t;
typedef struct scsi_inquiry scsi_inquiry_t;
typedef struct scsi_capacity scsi_capacity_t;

#define SCSI_CMD_READ_CAPACITY  0x25
#define SCSI_CMD_READ           0xa8
#define SCSI_CMD_INQUIRY        0x12
#define SCSI_CMD_EJECT          0x1b

struct scsi_packet
{
    uint8_t data[12];
};

struct scsi_capacity
{
    u32_msb_t lba;
    u32_msb_t block_size;
};

struct scsi_inquiry
{
    uint8_t   peri_device_type:5;
    uint8_t   peri_qualifier:3;
    uint8_t   reserved_1:7;
    uint8_t   rmb:1;
    uint8_t   version;
    uint8_t   resp_data_fmt:4;
    uint8_t   hiusp:1;
    uint8_t   normaca:1;
    uint8_t   obsolete:2;
    uint8_t   additional_len;
    uint8_t   flags[3];
    char      vendor_id[8];
    char      product_id[16];
    u32_msb_t product_rev_lvl;
};

#define SCSI_READ_PACKET(lba, sectors) \
    (scsi_packet_t){ \
        .data = { \
            SCSI_CMD_READ, \
            0, \
            (lba >> 24) & 0xff, \
            (lba >> 16) & 0xff, \
            (lba >> 8) & 0xff, \
            (lba >> 0) & 0xff, \
            0, \
            0, \
            0, \
            sectors, \
            0, \
            0, \
        } \
    }

#define SCSI_READ_CAPACITY_PACKET() \
    (scsi_packet_t){ \
        .data = { \
            SCSI_CMD_READ_CAPACITY, \
        } \
    }

#define SCSI_INQUIRY_PACKET(len) \
    (scsi_packet_t){ \
        .data = { \
            SCSI_CMD_INQUIRY, \
            0, \
            0, \
            ((len) >> 8) & 0xff, \
            ((len) >> 0) & 0xff, \
        } \
    }
