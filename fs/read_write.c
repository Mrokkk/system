#include <kernel/process.h>
#include <kernel/fs.h>

/*===========================================================================*
 *                                  sys_write                                *
 *===========================================================================*/
int sys_write(int fd, const char *buffer, size_t size) {

    struct file *file;
    char kbuf[size+1];

    memcpy_from_user(kbuf, buffer, size);

    process_fd_get(process_current, fd, &file);

    if (!file) return -EBADF;
    if (!file->ops) return -ENODEV;
    if (!file->ops->write) return -ENODEV;

    return file->ops->write(file->inode, file, kbuf, size);

}

/*===========================================================================*
 *                                  sys_read                                 *
 *===========================================================================*/
int sys_read(int fd, char *buffer, size_t n) {

    struct file *file;
    char kbuf[n+1];
    int res;

    process_fd_get(process_current, fd, &file);

    if (!file) return -EBADF;
    if (!file->ops) return -ENODEV;
    if (!file->ops->read) return -ENODEV;
    res = file->ops->read(file->inode, file, kbuf, n);

    memcpy_to_user(buffer, kbuf, n);

    return res;

}

