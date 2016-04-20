#include <kernel/process.h>
#include <kernel/device.h>
#include <kernel/fs.h>

/*===========================================================================*
 *                                  sys_open                                 *
 *===========================================================================*/
int sys_open(const char *filename, int mode) {

    char temp[64];
    int i = 0;
    int fd;

    for (i = 0; i < 64; i++) {
        get_from_user(&temp[i], (void *)&filename[i]);
        if (temp[i] == 0)
            break;
    }

    (void)mode;

    /* Temporary */
    if (!strncmp("/dev", temp, 4)) {

        struct file *file;
        struct device *dev = char_device_find(strrchr(temp, '/')+1);

        if (!dev) return -ENODEV;

        /* Find first free file descriptor */
        for (fd=0; fd<PROCESS_FILES; fd++)
            if (!process_current->files[fd]) break;

        if (fd == PROCESS_FILES - 1) goto open_error;

        if (!(file = kmalloc(sizeof(struct file))))
            return -ENOMEM;

        file->f_mode = mode;
        file->f_ops = dev->fops;

        /* Put file in system cache */
        file_put(file);

        process_current->files[fd] = file;

        if (!file->f_ops)
            return -ENODEV;
        if (!file->f_ops->open)
            return -ENODEV;

        return file->f_ops->open(0, file);

    }

    return -ENOENT;

    open_error: return -EMFILE;

}

/*===========================================================================*
 *                                  sys_dup                                  *
 *===========================================================================*/
int sys_dup(int fd) {

    int new_fd;

    if (fd >= PROCESS_FILES) return -EMFILE;

    /* Find first free file descriptor */
    for (new_fd=0; new_fd<PROCESS_FILES; new_fd++)
        if (!process_current->files[new_fd]) break;
    if (new_fd == PROCESS_FILES - 1) return -ENOMEM;

    if (!process_current->files[fd]) return -EBADF;

    process_current->files[new_fd] = process_current->files[fd];

    return 0;

}

/*===========================================================================*
 *                                 sys_close                                 *
 *===========================================================================*/
int sys_close(int fd) {

    struct file *file;
    int errno;

    if (fd >= PROCESS_FILES) return -EMFILE;

    file = process_current->files[fd];
    if (!file) return -EBADF;

    errno = file_free(file);

    process_current->files[fd] = 0;

    return errno;

}

