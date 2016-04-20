#ifndef __DEVICE_H_
#define __DEVICE_H_

#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/fs.h>
#include <kernel/dev.h>
#include <kernel/list.h>

#define MAJOR_CHR_TTY       4
#define MAJOR_CHR_SERIAL    5
#define MAJOR_BLK_FLOPPY    5

struct device {
    char name[32];
    unsigned char minor_max;
    struct kernel_module *owner;
    struct file_operations *fops;
    struct device *next;
    struct list_head *devices;
};

int __char_device_register(unsigned int major, const char *name, struct file_operations *fops, unsigned int this_module);
struct device *char_device_get(unsigned int major);
struct file_operations *char_fops_get(unsigned int major);
int char_devices_list_get(char *buffer);

int __block_device_register(unsigned int major, const char *name, struct file_operations *fops, unsigned int this_module);
struct device *block_device_get(unsigned int major);
struct file_operations *block_fops_get(unsigned int major);
int block_devices_list_get(char *buffer);

#define char_device_register(major, name, fops) \
    __char_device_register(major, name, fops, (unsigned int)&this_module)

#define block_device_register(major, name, fops) \
    __block_device_register(major, name, fops, (unsigned int)&this_module)

struct device *char_device_find(const char *name);

#endif /* __DEVICE_H_ */
