#ifndef INCLUDE_KERNEL_FS_H_
#define INCLUDE_KERNEL_FS_H_

#include <kernel/kernel.h>
#include <kernel/dev.h>
#include <kernel/dirent.h>
#include <kernel/wait.h>
#include <kernel/list.h>

typedef unsigned int off_t;

#define MAX_INODES 512

#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

#define READ 0
#define WRITE 1

struct super_block;

struct inode {
    dev_t dev;
    unsigned int ino;
    unsigned short mode;
    unsigned int nlinks;
    unsigned int uid;
    unsigned int gid;
    unsigned int size;
    unsigned int atime;
    unsigned int mtime;
    unsigned int ctime;
    struct super_block *sb;
    struct inode *next, *prev;
    struct inode *hash_next, *hash_prev;
    struct inode *bound_to, *bound_by;
    struct inode *mount;
    struct socket *socket;
    unsigned short count;
    unsigned short flags;
    unsigned char lock;
    unsigned char dirt;
    unsigned char pipe;
    unsigned char seek;
    unsigned char update;
    void *data;
    struct inode_operations {
        struct file_operations *default_file_ops;
        int (*create)(struct inode *, const char *, int, int, struct inode **);
        int (*lookup)(struct inode *, const char *, int, struct inode **);
        int (*link)(struct inode *, struct inode *, const char *,int);
        int (*unlink)(struct inode *, const char *, int);
        int (*symlink)(struct inode *, const char *, int, const char *);
        int (*mkdir)(struct inode *, const char *, int, int);
        int (*rmdir)(struct inode *, const char *, int);
        int (*mknod)(struct inode *, const char *, int, int, int);
        int (*rename)(struct inode *, const char *, int, struct inode *, const char *, int);
        int (*readlink)(struct inode *, char *, int);
        int (*follow_link)(struct inode *, struct inode *, int, int, struct inode **);
        int (*bmap)(struct inode *, int);
        void (*truncate)(struct inode *);
        int (*permission)(struct inode *, int);
    } *ops;
};

struct file {
    unsigned short mode;
    dev_t rdev;
    off_t pos;
    unsigned short flags;
    unsigned short count;
    unsigned short reada;
    struct inode *inode;
    struct list_head files;
    struct file_operations {
        int (*lseek)(struct inode *, struct file *, off_t, int);
        int (*read)(struct inode *, struct file *, char *, int);
        int (*write)(struct inode *, struct file *, char *, int);
        int (*readdir)(struct inode *, struct file *, struct dirent *, int);
        //int (*select)(struct inode *, struct file *, int, select_table *);
        int (*ioctl)(struct inode *, struct file *, unsigned int, unsigned long);
        int (*mmap)(struct inode *, struct file *, unsigned long, size_t, int, unsigned long);
        int (*open)(struct inode *, struct file *);
        void (*release)(struct inode *, struct file *);
        int (*fsync)(struct inode *, struct file *);
    } *ops;
};

struct super_block {
    dev_t dev;
    unsigned long blocksize;
    unsigned char blocksize_bits;
    unsigned char lock;
    unsigned char rd_only;
    unsigned char dirt;
    unsigned long flags;
    unsigned long magic;
    unsigned long time;
    struct inode *covered;
    struct inode *mounted;
    struct wait_queue *wait;
    void *data;
    struct super_operations {
        void (*read_inode)(struct inode *);
        int (*notify_change)(int flags, struct inode *);
        void (*write_inode)(struct inode *);
        void (*put_inode)(struct inode *);
        void (*put_super)(struct super_block *);
        void (*write_super)(struct super_block *);
        //void (*statfs)(struct super_block *, struct statfs *);
        int (*remount_fs)(struct super_block *, int *, char *);
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

/*===========================================================================*
 *                                file_close                                 *
 *===========================================================================*/
static inline void file_free(struct file *file) {

    list_del(&file->files);
    kfree(file);

}

#endif /* INCLUDE_KERNEL_FS_H_ */
