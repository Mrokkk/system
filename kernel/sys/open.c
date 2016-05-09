#include <kernel/process.h>
#include <kernel/device.h>
#include <kernel/fs.h>

#include <arch/segment.h>

/*===========================================================================*
 *                                  sys_open                                 *
 *===========================================================================*/
int sys_open(const char *filename, int mode) {

    char kbuf[64];
    int fd, errno;
    struct file *file;

    strcpy_from_user(kbuf, filename);

    if (process_find_free_fd(process_current, &fd))
        return fd;
    if ((errno = do_open(&file, filename, mode)))
        return errno;
    process_fd_set(process_current, fd, file);
    file->count = 1;

    return 0;

}

/*===========================================================================*
 *                                  sys_dup                                  *
 *===========================================================================*/
int sys_dup(int fd) {

    int new_fd;
    struct file *file;

    if (fd_check_bounds(fd))
        return -EMFILE;
    if (process_find_free_fd(process_current, &new_fd))
        return -ENOMEM;
    if (process_fd_get(process_current, fd, &file))
        return -EBADF;
    process_fd_set(process_current, new_fd, file);
    file->count++;

    return 0;

}

/*===========================================================================*
 *                                 sys_close                                 *
 *===========================================================================*/
int sys_close(int fd) {

    struct file *file;

    if (fd_check_bounds(fd))
        return -EMFILE;
    if (process_fd_get(process_current, fd, &file))
        return -EBADF;
    process_fd_set(process_current, fd, 0);

    if (!--file->count)
       DESTRUCT(file, list_del(&file->files));

    return 0;

}

