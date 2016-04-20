#include <kernel/process.h>
#include <kernel/device.h>

/*===========================================================================*
 *                                  sys_write                                *
 *===========================================================================*/
int sys_write(int fd, const char *buffer, size_t size) {

    struct file *file;
    char kbuf[size+1];

    copy_from_user(kbuf, (void *)buffer, size);

    file = process_current->files[fd];

    if (!file) return -EBADF;
    if (!file->f_ops) return -ENODEV;
    if (!file->f_ops->write) return -ENODEV;

    return file->f_ops->write(0, file, kbuf, size);

}
