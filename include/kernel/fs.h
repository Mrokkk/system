#pragma once

#include <kernel/dev.h>
#include <kernel/list.h>
#include <kernel/wait.h>
#include <kernel/magic.h>
#include <kernel/dentry.h>
#include <kernel/dirent.h>
#include <kernel/kernel.h>
#include <kernel/compiler.h>

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
struct inode_operations;
struct super_operations;

struct inode
{
    MAGIC_NUMBER;
    dev_t dev;
    ino_t ino;
    size_t size;
    struct super_block* sb;
    struct list_head list;
    struct file_operations* file_ops;
    struct inode_operations* ops;
    void* fs_data;
};

typedef struct inode inode_t;

struct inode_operations
{
    int (*lookup)(inode_t* parent, const char* name, inode_t** result);
    int (*create)(inode_t* parent, const char* name, int, int, inode_t** result);
    int (*mkdir)(inode_t* parent, const char* name, int, int, inode_t** result);
};

struct file
{
    MAGIC_NUMBER;
    unsigned short mode;
    unsigned short count;
    inode_t* inode;
    struct list_head files;
    struct file_operations* ops;
};

typedef struct file file_t;

typedef int (*direntadd_t)(void* buf, const char* name, size_t name_len, ino_t ino, char type);

struct file_operations
{
    int (*read)(file_t* file, char* buf, int count);
    int (*write)(file_t* file, const char* buf, int count);
    int (*readdir)(file_t* file, void* buf, direntadd_t dirent_add);
    int (*mmap)(file_t* file, void** data); // FIXME: incorrect arguments
    int (*open)(file_t* file);
};

struct super_block
{
    dev_t dev;
    inode_t* mounted;
    void* data;
    struct list_head super_blocks;

    // Those are filled by specific fs during mount() call
    unsigned int module;
    struct super_operations* ops;
};

typedef struct super_block super_block_t;

struct super_operations
{
};

struct file_system
{
    const char* name;
    struct list_head file_systems;
    struct list_head super_blocks;

    // Fills missing params of super block (module, ops) and root inode;
    // returns errno
    int (*mount)(super_block_t* sb, inode_t* inode, void*, int);
};

struct mounted_system
{
    char* dir;
    dev_t dev;
    struct super_block* sb;
    struct file_system* fs;
    struct list_head mounted_systems;
};

typedef struct file_system file_system_t;
typedef struct mounted_system mounted_system_t;

extern struct list_head files;
extern struct list_head mounted_inodes;
extern inode_t* root;

int file_system_register(file_system_t* fs);
dentry_t* lookup(const char* filename);
int do_mount(file_system_t* fs, const char* mount_point);
int do_open(file_t** new_file, const char* filename, int flags, int mode);

int inode_get(inode_t** inode);
int inode_put(inode_t* inode);
char* inode_print(const void* data, char* str);

void file_systems_print(void);
