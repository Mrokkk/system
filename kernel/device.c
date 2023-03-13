#include <kernel/device.h>

struct device* char_devices[CHAR_DEVICES_SIZE];

int __char_device_register(
    unsigned int major,
    const char* name,
    struct file_operations* fops,
    dev_t max_minor,
    void* private,
    unsigned int this_module)
{
    struct device* dev;
    struct kernel_module* mod;

    if (new(dev))
    {
        return -ENOMEM;
    }

    dev->fops = fops;
    dev->owner = 0;
    dev->max_minor = max_minor;
    dev->private = private;
    strcpy(dev->name, name);

    if ((mod = module_find(this_module)))
    {
        dev->owner = mod;
    }

    char_devices[major] = dev;

    return 0;
}

int char_devices_list_get(char* buffer)
{
    int len = sprintf(buffer, "Installed char devices: \n");

    for (int i = 0; i < CHAR_DEVICES_SIZE; i++)
    {
        if (char_devices[i])
        {
            len += sprintf(buffer+len, "%u %s\n", i, char_devices[i]->name);
        }
    }

    return len;
}

int char_device_find(const char* name, struct device** dev)
{
    *dev = 0;
    for (int i = 0; i < CHAR_DEVICES_SIZE; i++)
    {
        if (!char_devices[i])
        {
            continue;
        }
        if (!strncmp(char_devices[i]->name, name, CHAR_DEVICE_NAME_SIZE))
        {
            *dev = char_devices[i];
            return i;
        }
    }

    return 0;
}
