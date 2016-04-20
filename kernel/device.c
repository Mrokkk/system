#include <kernel/device.h>

struct device *char_devices[16];
struct device *block_devices[16];

/* TODO: This is piece of crap */

/*===========================================================================*
 *                           char_register_device                            *
 *===========================================================================*/
int __char_device_register(unsigned int major, const char *name,
                           struct file_operations *fops,
                           unsigned int this_module) {

    struct device *new, *temp = char_devices[major];
    struct kernel_module *mod;

    new = kmalloc(sizeof(struct device));
    if (unlikely(!new)) return -ENOMEM; /* Can't allocate memory... */

    new->fops = fops;
    new->next = 0;
    strcpy(new->name, name);

    new->owner = 0;
    mod = module_find(this_module);

    if (mod)
        new->owner = mod;

    if (temp == 0) {
        char_devices[major] = new;
        return 0;
    }

    for (; temp->next != 0; temp = temp->next);

    temp->next = new;

    return 0;

}

/*===========================================================================*
 *                              char_device_get                              *
 *===========================================================================*/
struct device *char_device_get(unsigned int major) {

    return char_devices[major];

}

/*===========================================================================*
 *                              char_fops_get                                *
 *===========================================================================*/
struct file_operations *char_fops_get(unsigned int major) {

    return char_devices[major]->fops;

}

/*===========================================================================*
 *                           char_devices_list_get                           *
 *===========================================================================*/
int char_devices_list_get(char *buffer) {

    int major, minor, len = 0;
    struct device *temp;

    len = sprintf(buffer, "Installed char devices: \n");

    for (major=0; major<16; major++) {
        for (minor=0, temp=char_devices[major];
             temp != 0; minor++, temp=temp->next)
            len += sprintf(buffer+len, "%u %u %s\n",
                    major, minor, temp->name);
    }

    return len;

}

/*===========================================================================*
 *                             char_device_find                              *
 *===========================================================================*/
struct device *char_device_find(const char *name) {

    struct device *temp;
    int major, minor;

    for (major=0; major<16; major++) {
        for (minor=0, temp=char_devices[major];
             temp != 0; minor++, temp=temp->next)
            if (!strcmp(temp->name, name)) return temp;
    }

    return 0;
}

/*===========================================================================*
 *                           block_register_device                           *
 *===========================================================================*/
int __block_device_register(unsigned int major, const char *name,
                            struct file_operations *fops,
                            unsigned int this_module) {

    struct device *new, *temp = block_devices[major];
    struct kernel_module *mod;

    kernel_trace("adding block device %u %s", major, name);

    new = kmalloc(sizeof(struct device));
    if (unlikely(!new)) return -ENOMEM;

    new->fops = fops;
    new->next = 0;
    strcpy(new->name, name);
    new->owner = 0;
    mod = module_find(this_module);
    if (mod)
        new->owner = mod;

    if (temp == 0) {
        block_devices[major] = new;
        return 0;
    }

    for (; temp->next != 0; temp = temp->next);

    temp->next = new;

    return 0;

}

/*===========================================================================*
 *                             block_device_get                              *
 *===========================================================================*/
struct device *block_device_get(unsigned int major) {

    return block_devices[major];

}

/*===========================================================================*
 *                              block_fops_get                               *
 *===========================================================================*/
struct file_operations *block_fops_get(unsigned int major) {

    return block_devices[major]->fops;

}

/*===========================================================================*
 *                          block_devices_list_get                           *
 *===========================================================================*/
int block_devices_list_get(char *buffer) {

    int major, minor, len = 0;
    struct device *temp;

    len = sprintf(buffer, "Installed block devices: \n");

    for (major=0; major<16; major++) {
        for (minor=0, temp=block_devices[major]; temp != 0; minor++, temp=temp->next)
            len += sprintf(buffer+len, "%u %u %s\n", major, minor, temp->name);
    }

    return len;

}
