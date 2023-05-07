#include <arch/io.h>

#include <kernel/page.h>
#include <kernel/devfs.h>
#include <kernel/string.h>
#include <kernel/device.h>
#include <kernel/module.h>

#include "tty.h"
#include "tty_driver.h"

#define MINOR_NULL 1

KERNEL_MODULE(mem);
module_init(mem_init);
module_exit(mem_deinit);

static int mem_open(file_t* file);
static int null_write(file_t* file, const char* buffer, size_t size);
static int null_read(file_t* file, char* buffer, size_t count);

static file_operations_t fops = {
    .open = &mem_open,
};

UNMAP_AFTER_INIT static int mem_init()
{
    int errno;

    if ((errno = char_device_register(MAJOR_CHR_MEM, "mem", &fops, 1, NULL)))
    {
        log_warning("failed to register char device: %d", errno);
        return errno;
    }

    if ((errno = devfs_register("null", MAJOR_CHR_MEM, MINOR_NULL)))
    {
        log_error("failed to register null to devfs: %d", errno);
        return errno;
    }

    return 0;
}

static int mem_deinit()
{
    return 0;
}

static int mem_open(file_t* file)
{
    switch (MINOR(file->inode->dev))
    {
        case MINOR_NULL:
            file->ops->read = &null_read;
            file->ops->write = &null_write;
            break;
        default:
            return -ENODEV;
    }

    return 0;
}

static int null_write(file_t*, const char*, size_t)
{
    return 0;
}

static int null_read(file_t*, char*, size_t)
{
    return 0;
}
