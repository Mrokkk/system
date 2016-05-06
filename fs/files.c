#include <kernel/kernel.h>
#include <kernel/fs.h>

LIST_DECLARE(files);

struct file *file_create() {

    struct file *fil;

    if ((fil = kmalloc(sizeof(struct file))) == 0) return 0;

    list_add_tail(&fil->files, &files);

    return fil;

}
