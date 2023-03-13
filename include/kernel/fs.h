#pragma once

#include <kernel/dev.h>
#include <kernel/list.h>
#include <kernel/wait.h>
#include <kernel/magic.h>
#include <kernel/dentry.h>
#include <kernel/dirent.h>
#include <kernel/kernel.h>

#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

#define O_RDONLY            0
#define O_WRONLY            1
#define O_RDWR              2
#define O_CREAT          0x40
#define O_EXCL           0x80
#define O_NOCTTY        0x100
#define O_TRUNC         0x200
#define O_APPEND        0x400
#define O_NONBLOCK      0x800
#define O_DIRECTORY   0x10000

struct inode;
struct super_block;
struct file_operations;

struct inode_operations
{
    int (*create)(struct inode*, const char*, int, int, struct inode**);
    int (*lookup)(struct inode*, const char*, int, struct inode**);
    //int (*link)(struct inode*, struct inode*, const char*,int);
    //int (*unlink)(struct inode*, const char*, int);
    //int (*symlink)(struct inode*, const char*, int, const char*);
    int (*mkdir)(struct inode*, const char*, int, int, struct inode**);
    //int (*rmdir)(struct inode*, const char*, int);
    //int (*mknod)(struct inode*, const char*, int, int, int);
    //int (*rename)(struct inode*, const char*, int, struct inode*, const char*, int);
    //int (*readlink)(struct inode*, char*, int);
    //int (*follow_link)(struct inode*, struct inode*, int, int, struct inode**);
    //int (*bmap)(struct inode*, int);
    //void (*truncate)(struct inode*);
    //int (*permission)(struct inode*, int);
};

struct inode
{
    MAGIC_NUMBER;
    dev_t dev;
    unsigned int ino;
    //unsigned short mode;
    //unsigned int nlinks;
    //unsigned int uid;
    //unsigned int gid;
    size_t size;
    //unsigned int atime;
    //unsigned int mtime;
    //unsigned int ctime;
    struct super_block* sb;
    //struct inode* next;
    //struct inode* prev;
    //struct inode* hash_next;
    //struct inode* hash_prev;
    //struct inode* bound_to;
    //struct inode* bound_by;
    //struct inode* mount;
    //struct socket* socket;
    //unsigned short count;
    //unsigned short flags;
    //unsigned char lock;
    //unsigned char dirt;
    //unsigned char pipe;
    //unsigned char seek;
    //unsigned char update;
    struct file_operations* file_ops;
    struct inode_operations* ops;
    void* fs_data;
};

typedef struct inode inode_t;

typedef struct file
{
    MAGIC_NUMBER;
    unsigned short mode;
    //dev_t rdev;
    //k_off_t pos;
    //unsigned short flags;
    unsigned short count;
    //unsigned short reada;
    inode_t* inode;
    struct list_head files;
    struct file_operations
    {
        //int (*lseek)(struct inode *, struct file *, k_off_t, int);
        int (*read)(struct file*, char*, int);
        int (*write)(struct file*, char*, int);
        int (*readdir)(struct file*, struct dirent*, int);
        //int (*select)(struct inode *, struct file *, int, select_table *);
        //int (*ioctl)(struct inode *, struct file *, unsigned int, unsigned long);
        int (*mmap)(struct file*, void** data); // FIXME: incorrect arguments
        int (*open)(struct file*);
        //void (*release)(struct inode *, struct file *);
        //int (*fsync)(struct inode *, struct file *);
    } *ops;
} file_t;

typedef struct super_block
{
    MAGIC_NUMBER;
    dev_t dev;
    unsigned long blocksize;
    unsigned char blocksize_bits;
    unsigned char lock;
    unsigned char rd_only;
    unsigned char dirt;
    unsigned long flags;
    unsigned long time;
    inode_t* covered;
    inode_t* mounted;
    struct wait_queue* wait;
    void* data;
    unsigned int module;
    struct super_operations
    {
        void (*read_inode)(struct inode*);
        //int (*notify_change)(int flags, struct inode*);
        //void (*write_inode)(struct inode*);
        //void (*put_inode)(struct inode*);
        //void (*put_super)(struct super_block*);
        //void (*write_super)(struct super_block*);
        //void (*statfs)(struct super_block*, struct statfs*);
        //int (*remount_fs)(struct super_block*, int*, char*);
    } *ops;
    struct list_head super_blocks;
} super_block_t;

typedef struct file_system
{
    struct super_block* (*read_super)(struct super_block*, void*, int);
    char* name;
    int requires_dev;
    struct list_head file_systems;
    struct list_head super_blocks;
} file_system_t;

typedef struct mounted_system
{
    dev_t dev;
    char* dir;
    struct super_block* sb;
    struct list_head mounted_systems;
} mounted_system_t;

extern struct list_head files;
extern struct list_head mounted_inodes;
extern inode_t* root;

int vfs_init();
int file_system_register(file_system_t* fs);
dentry_t* lookup(const char* filename);
int do_mount(file_system_t* fs, const char* mount_point);
int do_open(file_t** new_file, const char* filename, int flags, int mode);
file_t* file_create();

void inode_init(inode_t* inode);
void mountpoints_print(void);
