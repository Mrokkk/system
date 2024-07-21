#pragma once

#include <kernel/api/types.h>

#define MINOR(dev) ((dev) & 0xff)
#define MAJOR(dev) ((dev) >> 8)
#define MKDEV(maj, min) (((maj) << 8) | (min))
#define NODEV    0xffff

#define MAJOR_CHR_FB        3
#define MAJOR_CHR_TTY       4
#define MAJOR_CHR_TTYAUX    5
#define MAJOR_CHR_SERIAL    6
#define MAJOR_CHR_DEBUG     7
#define MAJOR_CHR_MOUSE     8
#define MAJOR_CHR_MEM       9

#define MAJOR_BLK_DISKIMG   256
#define MAJOR_BLK_IDE       257
#define MAJOR_BLK_AHCI      258

#define BLK_NO_PARTITION                0xf
#define BLK_MINOR_DRIVE(drive)          (drive | (BLK_NO_PARTITION << 4))
#define BLK_MINOR(partition, drive)     ((drive) | (partition << 4))
#define BLK_PARTITION(minor)            (((minor) >> 4) & 0xf)
#define BLK_DRIVE(minor)                ((minor) & 0xf)
