#include <kernel/process.h>
#include <kernel/device.h>

/*===========================================================================*
 *                                  sys_write                                *
 *===========================================================================*/
int sys_write(int fd, const char *buffer, size_t size) {

    struct file *file;
    char kbuf[size+1];

    memcpy_from_user(kbuf, buffer, size);

    file = process_current->files[fd];

    if (!file) return -EBADF;
    if (!file->ops) return -ENODEV;
    if (!file->ops->write) return -ENODEV;

    return file->ops->write(0, file, kbuf, size);

}
