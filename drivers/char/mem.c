#include <arch/io.h>

#include <kernel/fs.h>
#include <kernel/page.h>
#include <kernel/devfs.h>
#include <kernel/string.h>
#include <kernel/module.h>

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

    if ((errno = devfs_register("null", MAJOR_CHR_MEM, MINOR_NULL, &fops)))
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
    switch (MINOR(file->inode->rdev))
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
