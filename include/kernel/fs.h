#ifndef INCLUDE_KERNEL_FS_H_
#define INCLUDE_KERNEL_FS_H_

#include <kernel/kernel.h>
#include <kernel/dev.h>
#include <kernel/dirent.h>
#include <kernel/wait.h>
#include <kernel/list.h>

typedef unsigned int off_t;

#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

#define READ 0
#define WRITE 1

struct super_block;

struct inode {
    dev_t device_id;
    struct super_block *super_block;
    struct inode_operations {
        struct file_operations *file_ops;
        int (*create)(struct inode *, const char *, int, int, struct inode **);
        int (*lookup)(struct inode *, const char *, int, struct inode **);
        int (*mkdir)(struct inode *, const char *, int, int);
        int (*rmdir)(struct inode *, const char *, int);
        int (*rename)(struct inode *, const char *, int, struct inode *, const char *, int);
    } *ops;
};

struct file {
    struct inode *inode;
    struct list_head files;
    struct file_operations {
        int (*lseek)(struct inode *, struct file *, off_t, int);
        int (*read)(struct inode *, struct file *, char *, int);
        int (*write)(struct inode *, struct file *, char *, int);
        int (*readdir)(struct inode *, struct file *, struct dirent *, int);
        int (*open)(struct inode *, struct file *);
    } *ops;
};

struct super_block {
    dev_t device_id;
    struct inode *covered;
    struct inode *mounted;
    void *data;
    struct super_operations {
        void (*read_inode)(struct inode *);
        void (*write_inode)(struct inode *);
    } *ops;
    struct list_head super_blocks;
};

struct file_system {
    struct super_block *(*read_super)(struct super_block *, void *, int);
    char *name;
    int requires_dev;
    struct list_head file_systems;
    struct list_head super_blocks;
};

struct mounted_system {
    dev_t dev;
    char *dir;
    struct super_block *sb;
    struct list_head mounted_systems;
};

extern struct list_head files;
extern struct list_head mounted_inodes;
extern struct inode *root;

int vfs_init();
int file_system_register(struct file_system *fs);
struct inode *lookup(const char *filename);
int do_mount(struct file_system *fs, const char *mount_point);
int do_open(struct file **new_file, const char *filename, int mode);
struct file *file_create();

#endif /* INCLUDE_KERNEL_FS_H_ */
