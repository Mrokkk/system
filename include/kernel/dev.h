#pragma once

#include <kernel/api/dev.h>

#define MINOR(dev)          _MINOR(dev)
#define MAJOR(dev)          _MAJOR(dev)
#define MKDEV(maj, min)     _MKDEV(maj, min)

#define MAJOR_CHR_EARLYCON        2
#define MAJOR_CHR_FB              3
#define MAJOR_CHR_TTY             4
#define MAJOR_CHR_TTYAUX          5
#define MAJOR_CHR_SERIAL          6
#define MAJOR_CHR_DEBUG           7
#define MAJOR_CHR_MOUSE           8
#define MAJOR_CHR_MEM             9
#define MAJOR_CHR_VIRTIO_CONSOLE  10

#define MAJOR_BLK_IDE             256
#define MAJOR_BLK_SATA            257
#define MAJOR_BLK_CDROM           258

#define BLK_NO_PARTITION                -1
#define BLK_MINOR_DRIVE(drive)          ((drive) << 4)
#define BLK_MINOR(partition, drive)     (((drive) << 4) | ((partition) + 1))
#define BLK_PARTITION(minor)            (((minor) & 0xf) - 1)
#define BLK_DRIVE(minor)                (((minor) >> 4) & 0xf)
