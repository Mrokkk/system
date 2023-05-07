#pragma once

#include <kernel/fs.h>
#include <kernel/dev.h>
#include <kernel/list.h>
#include <kernel/kernel.h>
#include <kernel/module.h>

#define CHAR_DEVICES_SIZE   16
#define BLK_DEVICES_SIZE    16
#define DEVICE_NAME_SIZE    32

#define MAJOR_CHR_FB        3
#define MAJOR_CHR_TTY       4
#define MAJOR_CHR_SERIAL    5
#define MAJOR_CHR_DEBUG     6
#define MAJOR_CHR_MEM       7
#define MAJOR_BLK_IDE       8
#define MAJOR_BLK_DISKIMG   9

#define BLK_NO_PARTITION                0xf
#define BLK_MINOR_DRIVE(drive)          (drive | (BLK_NO_PARTITION << 4))
#define BLK_MINOR(partition, drive)     ((drive) | (partition << 4))
#define BLK_PARTITION(minor)            (((minor) >> 4) & 0xf)
#define BLK_DRIVE(minor)                ((minor) & 0xf)

struct device;
typedef struct device device_t;

struct device
{
    char name[DEVICE_NAME_SIZE];
    struct kernel_module* owner;
    struct file_operations* fops;
    dev_t major;
    void* private;
};

int __char_device_register(
    unsigned int major,
    const char* name,
    struct file_operations* fops,
    void* private,
    unsigned int this_module);

int char_device_find(const char* name, device_t** dev);

static inline device_t* char_device_get(dev_t major)
{
    extern device_t* char_devices[CHAR_DEVICES_SIZE];
    return char_devices[major];
}

static inline struct file_operations* char_fops_get(dev_t major)
{
    extern device_t* char_devices[CHAR_DEVICES_SIZE];
    return char_devices[major]->fops;
}

#define char_device_register(major, name, fops, max_minor, private) \
    __char_device_register(major, name, fops, private, this_module)
