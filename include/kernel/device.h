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
    struct kernel_module *owner;
    struct file_operations *fops;
};

int __char_device_register(unsigned int major, const char *name, struct file_operations *fops, unsigned int this_module);
int char_devices_list_get(char *buffer);
int char_device_find(const char *name, struct device **dev);

extern struct device *char_devices[16];

static inline struct device *char_device_get(unsigned int major) {
    return char_devices[major];
}

static inline struct file_operations *char_fops_get(unsigned int major) {
    return char_devices[major]->fops;
}

#define char_device_register(major, name, fops) \
    __char_device_register(major, name, fops, (unsigned int)&this_module)

#endif /* __DEVICE_H_ */
