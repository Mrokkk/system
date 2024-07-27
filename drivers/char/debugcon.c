#include <arch/io.h>

#include <kernel/fs.h>
#include <kernel/page.h>
#include <kernel/devfs.h>
#include <kernel/string.h>
#include <kernel/module.h>

#define PORT 0xe9
#define debugcon_send(c) outb(c, PORT)

KERNEL_MODULE(debugcon);
module_init(debugcon_init);
module_exit(debugcon_deinit);

static int debugcon_open(file_t* file);
int debugcon_write(file_t* file, const char* buffer, size_t size);

static file_operations_t fops = {
    .open = &debugcon_open,
    .write = &debugcon_write,
};

UNMAP_AFTER_INIT static int debugcon_init()
{
    int errno;

    if ((errno = devfs_register("debug0", MAJOR_CHR_DEBUG, 0, &fops)))
    {
        log_error("failed to register null to devfs: %d", errno);
        return errno;
    }

    return 0;
}

static int debugcon_deinit()
{
    return 0;
}

static int debugcon_open(file_t*)
{
    return 0;
}

int debugcon_write(file_t*, const char* buffer, size_t size)
{
    size_t old = size;
    while (size--)
    {
        debugcon_send(*buffer++);
    }
    return old;
}
