#ifndef INCLUDE_KERNEL_FS_H_
#define INCLUDE_KERNEL_FS_H_

#include <kernel/dev.h>
#include <kernel/dirent.h>
#include <kernel/wait.h>

typedef unsigned int off_t;

#define MAX_INODES 512

#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

#define READ 0
#define WRITE 1

struct inode_operations;
struct super_block;
struct file_operations;
struct super_operations;

struct inode {
    dev_t i_dev;
    unsigned int i_ino;
    unsigned short i_mode;
    unsigned int i_nlinks;
    unsigned int i_uid;
    unsigned int i_gid;
    unsigned int i_size;
    unsigned int i_atime;
    unsigned int i_mtime;
    unsigned int i_ctime;
    struct inode_operations *i_ops;
    struct super_block *i_sb;
    struct inode * i_next, * i_prev;
    struct inode * i_hash_next, * i_hash_prev;
    struct inode * i_bound_to, * i_bound_by;
    struct inode * i_mount;
    struct socket * i_socket;
    unsigned short i_count;
    unsigned short i_flags;
    unsigned char i_lock;
    unsigned char i_dirt;
    unsigned char i_pipe;
    unsigned char i_seek;
    unsigned char i_update;
    void *data;
};

struct file {
    unsigned short f_mode;
    dev_t f_rdev;
    off_t f_pos;
    unsigned short f_flags;
    unsigned short f_count;
    unsigned short f_reada;
    struct file *f_next, *f_prev;
    struct inode * f_inode;
    struct file_operations * f_ops;
};

struct super_block {
    dev_t s_dev;
    unsigned long s_blocksize;
    unsigned char s_blocksize_bits;
    unsigned char s_lock;
    unsigned char s_rd_only;
    unsigned char s_dirt;
    struct super_operations *s_op;
    unsigned long s_flags;
    unsigned long s_magic;
    unsigned long s_time;
    struct inode * s_covered;
    struct inode * s_mounted;
    struct wait_queue * s_wait;
    void *data;
};

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
};

struct inode_operations {
    struct file_operations * default_file_ops;
    int (*create)(struct inode *,const char *,int,int,struct inode **);
    int (*lookup)(struct inode *,const char *,int,struct inode **);
    int (*link)(struct inode *,struct inode *,const char *,int);
    int (*unlink)(struct inode *,const char *,int);
    int (*symlink)(struct inode *,const char *,int,const char *);
    int (*mkdir)(struct inode *,const char *,int,int);
    int (*rmdir)(struct inode *,const char *,int);
    int (*mknod)(struct inode *,const char *,int,int,int);
    int (*rename)(struct inode *,const char *,int,struct inode *,const char *,int);
    int (*readlink)(struct inode *,char *,int);
    int (*follow_link)(struct inode *,struct inode *,int,int,struct inode **);
    int (*bmap)(struct inode *,int);
    void (*truncate)(struct inode *);
    int (*permission)(struct inode *, int);
};

struct super_operations {
    void (*read_inode)(struct inode *);
    int (*notify_change)(int flags, struct inode *);
    void (*write_inode)(struct inode *);
    void (*put_inode)(struct inode *);
    void (*put_super)(struct super_block *);
    void (*write_super)(struct super_block *);
    //void (*statfs)(struct super_block *, struct statfs *);
    int (*remount_fs)(struct super_block *, int *, char *);
};

struct file_system_type {
    struct super_block *(*read_super)(struct super_block *, void *, int);
    char *name;
    int requires_dev;
};

int file_put(struct file *file);
int file_free(struct file *file);

#endif /* INCLUDE_KERNEL_FS_H_ */
