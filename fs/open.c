#include <kernel/fs.h>

static inline char *path_next(char *path) {

    return strchr(path, '/');

}

void base_name(char *path, char *name) {

    while (*path != '/' && *path != 0)
        *name++ = *path++;

    *name = 0;

}

struct inode *lookup(const char *filename) {

    char *path = (char *)filename;
    char name[255];
    struct inode *temp, *temp2;

    if (!root) return 0;

    if (*path == '/') path = path_next(path)+1;
    temp = root;

    do {
        base_name(path, name);
        temp->ops->lookup(temp, name, 0, &temp2);
        temp = temp2;
        path = path_next(path);
        if (!path) break;
        path++;
    } while (1);

    return temp2;

}

int do_open(struct file **new_file, const char *filename, int mode) {

    struct inode *inode;

    inode = lookup(filename);

    *new_file = file_create();
    (*new_file)->inode = inode;
    (*new_file)->ops = inode->ops->default_file_ops;
    (*new_file)->mode = mode;
    if (!(*new_file)->ops) {
        printk("File ops is NULL!\n");
        return -ENODEV;
    }
    if (!(*new_file)->ops->open) return -ENOOPS;
    (*new_file)->ops->open(inode, *new_file);

    return 0;

}
