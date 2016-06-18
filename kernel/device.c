#include <kernel/device.h>

struct device *char_devices[16];

/*===========================================================================*
 *                           char_register_device                            *
 *===========================================================================*/
int __char_device_register(unsigned int major, const char *name,
                           struct file_operations *fops,
                           unsigned int this_module) {

    struct device *dev;
    struct kernel_module *mod;

    if (new(dev)) return -ENOMEM;

    dev->fops = fops;
    dev->owner = 0;
    strcpy(dev->name, name);
    if ((mod = module_find(this_module))) dev->owner = mod;

    char_devices[major] = dev;

    return 0;

}

/*===========================================================================*
 *                           char_devices_list_get                           *
 *===========================================================================*/
int char_devices_list_get(char *buffer) {

    int i, len = 0;

    len = sprintf(buffer, "Installed char devices: \n");

    for (i=0; i<16; i++) {
        if (char_devices[i])
            len += sprintf(buffer+len, "%u %s\n",
                    i, char_devices[i]->name);
    }

    return len;

}

/*===========================================================================*
 *                             char_device_find                              *
 *===========================================================================*/
int char_device_find(const char *name, struct device **dev) {

    int i;

    *dev = 0;
    for (i=0; i<16; i++) {
        if (!char_devices[i]->name) continue;
        if (!strncmp(char_devices[i]->name, name, 32)) {
            *dev = char_devices[i];
            return i;
        }
    }

    return 0;
}
