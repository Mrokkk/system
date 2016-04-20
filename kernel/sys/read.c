#include <kernel/process.h>
#include <kernel/fs.h>

/*===========================================================================*
 *                                  sys_read                                 *
 *===========================================================================*/
int sys_read(int fd, char *buffer, size_t n) {

    struct file *file;
    char kbuf[n];
    int res;

    /* Get file corresponding to descriptor */
    file = process_current->files[fd];

    if (!file) return -EBADF;
    if (!file->f_ops) return -ENODEV;
    if (!file->f_ops->read) return -ENODEV;

    /* Call file-dependent function */
    res = file->f_ops->read(0, file, kbuf, n);

    copy_to_user(buffer, kbuf, n);

    return res;

}
