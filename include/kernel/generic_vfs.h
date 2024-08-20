#pragma once

#include <stddef.h>
#include <kernel/fs.h>

typedef struct generic_vfs_dir generic_vfs_dir_t;
typedef struct generic_vfs_entry generic_vfs_entry_t;

#define GENERIC_VFS_ROOT_INO 1

#define NODE(NAME, MODE, IOPS, FOPS) \
    { \
        .name = NAME, \
        .len = sizeof(NAME), \
        .mode = MODE, \
        .ino = GENERIC_VFS_ROOT_INO + 1 + __COUNTER__, \
        .iops = IOPS, \
        .fops = FOPS, \
    }

#define DIR(name, mode, iops, fops) \
    NODE(#name, S_IFDIR | mode, iops, fops)

#define REG(name, mode) \
    NODE(#name, S_IFREG | mode, &name##_iops, &name##_fops)

#define LNK(name, mode) \
    NODE(#name, S_IFLNK | mode, NULL, NULL)

#define DOT(name) \
    DIR(name, S_IFDIR | S_IRUGO, NULL, NULL)

struct generic_vfs_dir
{
    generic_vfs_entry_t* entries;
    size_t               count;
};

struct generic_vfs_entry
{
    const char*         name;
    size_t              len;
    mode_t              mode;
    ino_t               ino;
    inode_operations_t* iops;
    file_operations_t*  fops;
};

static inline generic_vfs_entry_t* generic_vfs_find(
    const char* name,
    generic_vfs_entry_t* dir,
    size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (!strcmp(name, dir[i].name))
        {
            return &dir[i];
        }
    }

    return NULL;
}
