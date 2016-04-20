#include <kernel/kernel.h>
#include <kernel/fs.h>

struct file *files;
int file_nr;

/*===========================================================================*
 *                                  file_put                                 *
 *===========================================================================*/
int file_put(struct file *file) {

    struct file *temp;

    if (!files) {
        files = file;
        file->f_next = 0;
        file->f_prev = 0;
        return 0;
    }

    for (temp=files; temp->f_next; temp=temp->f_next);

    temp->f_next = file;
    file->f_prev = temp;

    return 0;

}

/*===========================================================================*
 *                                file_close                                 *
 *===========================================================================*/
int file_free(struct file *file) {

    if (file->f_prev)
        file->f_prev->f_next = file->f_next;
    else
        files = file->f_next;
    kfree(file);

    return 0;

}

