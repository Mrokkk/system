#include <kernel/process.h>
#include <kernel/fs.h>

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
