#include <stdarg.h>
#include <kernel/fs.h>
#include <kernel/device.h>
#include <kernel/process.h>

device_t* char_devices[CHAR_DEVICES_SIZE];

int __char_device_register(
    unsigned int major,
    const char* name,
    struct file_operations* fops,
    void* private,
    unsigned int this_module)
{
    device_t* dev;
    kmod_t* mod;

    if (!(dev = alloc(device_t)))
    {
        return -ENOMEM;
    }

    dev->fops = fops;
    dev->owner = 0;
    dev->major = major;
    dev->private = private;
    strcpy(dev->name, name);

    if ((mod = module_find(this_module)))
    {
        dev->owner = mod;
    }

    char_devices[major] = dev;

    return 0;
}

int char_device_find(const char* name, device_t** dev)
{
    *dev = 0;
    for (int i = 0; i < CHAR_DEVICES_SIZE; i++)
    {
        if (!char_devices[i])
        {
            continue;
        }
        if (!strncmp(char_devices[i]->name, name, DEVICE_NAME_SIZE))
        {
            *dev = char_devices[i];
            return i;
        }
    }

    return 0;
}

int sys_ioctl(int fd, unsigned long request, ...)
{
    va_list args;
    file_t* file;
    void* arg;

    if (fd_check_bounds(fd) || process_fd_get(process_current, fd, &file))
    {
        return -EBADF;
    }

    va_start(args, request);
    arg = va_arg(args, void*);
    va_end(args);

    if (!file->ops || !file->ops->ioctl)
    {
        return -ENOSYS;
    }

    return file->ops->ioctl(file, request, arg);
}
