#pragma once

#include <stddef.h>
#include <kernel/vm.h>
#include <kernel/dev.h>
#include <kernel/list.h>
#include <kernel/page.h>
#include <kernel/poll.h>
#include <kernel/wait.h>
#include <kernel/magic.h>
#include <kernel/dentry.h>
#include <kernel/kernel.h>
#include <kernel/api/vfs.h>
#include <kernel/api/stat.h>
#include <kernel/api/fcntl.h>
#include <kernel/api/dirent.h>

#define BLOCK_SIZE       1024

typedef struct file file_t;
typedef struct inode inode_t;
typedef struct buffer buffer_t;
typedef struct fd_set fd_set_t;
typedef struct file_system file_system_t;
typedef struct super_block super_block_t;
typedef struct file_operations file_operations_t;
typedef struct block_operations block_operations_t;
typedef struct inode_operations inode_operations_t;
typedef struct super_operations super_operations_t;
typedef struct statfs statfs_t;

struct inode
{
    MAGIC_NUMBER;
    dev_t dev;
    ino_t ino;
    mode_t mode;
    uid_t uid;
    gid_t gid;
    time_t ctime;
    time_t mtime;
    size_t size;
    super_block_t* sb;
    list_head_t list;
    file_operations_t* file_ops;
    inode_operations_t* ops;
    void* fs_data;
};

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
    size_t offset;
    list_head_t files;
    file_operations_t* ops;
    void* private;
};

typedef int (*direntadd_t)(void* buf, const char* name, size_t name_len, ino_t ino, char type);

struct file_operations
{
    int (*read)(file_t* file, char* buf, size_t count);
    int (*write)(file_t* file, const char* buf, size_t count);
    int (*readdir)(file_t* file, void* buf, direntadd_t dirent_add);
    int (*mmap)(file_t* file, vm_area_t* vma, page_t* pages, size_t size, size_t offset);
    int (*ioctl)(file_t* file, unsigned long request, void* arg);
    int (*poll)(file_t* file, short events, short* revents, wait_queue_head_t** head);
    int (*open)(file_t* file);
    int (*close)(file_t* file);
};

struct super_block
{
    dev_t dev;
    inode_t* mounted;
    void* data;
    super_operations_t* ops;
    file_t* device_file;
    list_head_t super_blocks;

    // Those are filled by specific fs during mount() call
    unsigned int module;
    size_t block_size;
    void* fs_data;
};

struct super_operations
{
};

struct file_system
{
    const char* name;
    list_head_t file_systems;
    list_head_t super_blocks;

    // Fills missing params of super block (module, ops) and root inode;
    // returns errno
    int (*mount)(super_block_t* sb, inode_t* inode, void*, int);
};

struct mounted_system
{
    char* dir;
    char* device;
    dev_t dev;
    super_block_t* sb;
    file_system_t* fs;
    list_head_t mounted_systems;
};

typedef struct mounted_system mounted_system_t;

struct buffer
{
    list_head_t entry;
    size_t block;
    page_t* page;
    dev_t dev;
    size_t count;
    list_head_t buf_in_page;
    void* data;
};

extern list_head_t files;
extern list_head_t mounted_inodes;
extern inode_t* root;

int file_system_register(file_system_t* fs);
dentry_t* lookup(const char* filename);
int do_read(file_t* file, size_t offset, void* buffer, size_t count);
int do_mount(file_system_t* fs, const char* source, const char* mount_point, dev_t dev, file_t* device_file);
int do_open(file_t** new_file, const char* filename, int flags, int mode);
int do_close(file_t* file);

int inode_get(inode_t** inode);
int inode_put(inode_t* inode);
char* inode_print(const void* data, char* str);

void file_systems_print(void);

buffer_t* block_read(dev_t dev, file_t* file, uint32_t block);

static inline void __close(file_t** file)
{
    if (*file)
    {
        do_close(*file);
    }
}

#define scoped_file_t CLEANUP(__close) file_t

static inline char mode_to_type(mode_t mode)
{
    if (S_ISREG(mode)) return DT_REG;
    if (S_ISDIR(mode)) return DT_DIR;
    if (S_ISCHR(mode)) return DT_CHR;
    if (S_ISBLK(mode)) return DT_BLK;
    if (S_ISLNK(mode)) return DT_LNK;
    if (S_ISFIFO(mode)) return DT_FIFO;
    if (S_ISSOCK(mode)) return DT_SOCK;
    return DT_UNKNOWN;
}
