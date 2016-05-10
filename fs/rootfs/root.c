#include <kernel/kernel.h>
#include <kernel/fs.h>
#include <kernel/device.h>
#include <kernel/module.h>
#include <kernel/ctype.h>

KERNEL_MODULE(rootfs);
module_init(rootfs_init);
module_exit(rootfs_init);

struct super_block *rootfs_read_super(struct super_block *, void *, int);
void rootfs_read_inode(struct inode *);
int rootfs_lookup(struct inode *, const char *, int, struct inode **);
int rootfs_read(struct inode *, struct file *, char *, int);
int rootfs_write(struct inode *, struct file *, char *, int);
int rootfs_open(struct inode *, struct file *);

static struct file_system rootfs = {
        .name = "rootfs",
        .read_super = &rootfs_read_super,
};

static struct super_operations rootfs_sb_ops = {
        .read_inode = &rootfs_read_inode
};

static struct file_operations rootfs_fops = {
        .read = &rootfs_read,
        .write = &rootfs_write,
        .open = &rootfs_open,
};

static struct inode_operations rootfs_inode_ops = {
        .lookup = &rootfs_lookup,
        .default_file_ops = &rootfs_fops,
};

static struct inode_operations dev_ops;

struct super_block *rootfs_read_super(struct super_block *sb, void *data, int silent) {

    (void)sb; (void)data; (void)silent;

    sb->ops = &rootfs_sb_ops;

    return 0;

}

void rootfs_read_inode(struct inode *inode) {

    inode->ops = &rootfs_inode_ops;

}

int get_minor(const char *src) {

    char *temp;
    while (!isdigit(*src) && *src)
        src++;
    return strtol(src, &temp, 10);

}

void skip_minor(const char *src, char *target) {

    while (isalpha(*src) && (*target++ = *src++));
    *target = 0;

}

int rootfs_lookup(struct inode *dir, const char *name, int len, struct inode **result) {

    struct device *dev;
    char dev_name[32];
    int minor, major;

    (void)dir; (void)name; (void)len; (void)result; (void)minor;

    skip_minor(name, dev_name);
    major = char_device_find(dev_name, &dev);
    if ((dev) != 0) {
        minor = get_minor(name);
        dev_ops.default_file_ops = dev->fops;
        (*result)->ops = &dev_ops;
        (*result)->dev = MKDEV(major, minor);
    } else
        (*result)->ops = &rootfs_inode_ops;

    return 0;

}

int rootfs_read(struct inode *inode, struct file *file, char *buffer, int size) {

    (void)inode; (void)file; (void)buffer; (void)size;

    return 0;

}

int rootfs_write(struct inode *inode, struct file *file, char *buffer, int size) {

    (void)inode; (void)file; (void)buffer; (void)size;

    return 0;

}

int rootfs_open(struct inode *inode, struct file *file) {

    (void)inode; (void)file;

    printk("Bad open\n");

    return 0;

}

int rootfs_init() {

    file_system_register(&rootfs);

    return 0;

}
