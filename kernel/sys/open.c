#include <kernel/process.h>
#include <kernel/device.h>
#include <kernel/fs.h>

#include <arch/segment.h>

int process_find_free_fd(struct process *proc) {

    int fd;

    /* Find first free file descriptor */
    for (fd=0; fd<PROCESS_FILES; fd++)
        if (!proc->files[fd]) break;

    if (fd == PROCESS_FILES - 1) fd = -ENOMEM;

    return fd;

}

/*===========================================================================*
 *                                  sys_open                                 *
 *===========================================================================*/
int sys_open(const char *filename, int mode) {

    char kbuf[64];
    int fd, errno;
    struct file *file;

    strcpy_from_user(kbuf, filename);

    if ((fd = process_find_free_fd(process_current)) < 0)
        return fd;

    if ((errno = do_open(&file, filename, mode)))
        return errno;

    process_current->files[fd] = file;
    file->count = 1;

    return 0;

}

/*===========================================================================*
 *                                  sys_dup                                  *
 *===========================================================================*/
int sys_dup(int fd) {

    int new_fd;

    if (fd >= PROCESS_FILES) return -EMFILE;
    if ((new_fd = process_find_free_fd(process_current)) == -1)
        return -ENOMEM;

    if (!process_current->files[fd]) return -EBADF;

    process_current->files[new_fd] = process_current->files[fd];
    process_current->files[new_fd]->count++;

    return 0;

}

/*===========================================================================*
 *                                 sys_close                                 *
 *===========================================================================*/
int sys_close(int fd) {

    struct file *file;

    if (fd >= PROCESS_FILES) return -EMFILE;
    file = process_current->files[fd];
    if (!file) return -EBADF;

    process_current->files[fd] = 0;

    if (!--file->count)
       DESTRUCT(file, list_del(&file->files));

    return 0;

}

