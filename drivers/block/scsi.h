#pragma once

#include <stdint.h>

// References:
// https://www.seagate.com/files/staticfiles/support/docs/manual/Interface%20manuals/100293068j.pdf

typedef struct scsi_packet scsi_packet_t;

#define SCSI_CMD_READ_CAPACITY  0x25
#define SCSI_CMD_READ           0xa8
#define SCSI_CMD_EJECT          0x1b

struct scsi_packet
{
    uint8_t cmd;
    uint8_t data[11];
};

#define ATAPI_READ_PACKET(lba, sectors) \
    { \
        .cmd = SCSI_CMD_READ, \
        .data = { \
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

#define ATAPI_READ_CAPACITY_PACKET() \
    { \
        .cmd = SCSI_CMD_READ_CAPACITY, \
        .data = { \
        } \
    }
