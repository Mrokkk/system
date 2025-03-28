#include <kernel/fs.h>
#include <kernel/sysfs.h>
#include <kernel/string.h>
#include <kernel/segmexec.h>
#include <kernel/generic_vfs.h>

static int sysfs_mount(super_block_t* sb, inode_t* inode, void*, int);
static int sysfs_open(file_t* file);
static int sysfs_root_lookup(inode_t* dir, const char* name, inode_t** result);
static int sysfs_root_readdir(file_t* file, void* buf, direntadd_t dirent_add);

#define READDIR_END_OFFSET ((size_t)-1)

#define SYSFS_ENTRY_READ(name) \
    static int name##_read(file_t* file, char* buffer, size_t count) \
    { \
        int res = snprintf(buffer, count, "%u\n", sys_config.name); \
        if ((int)file->offset >= res) \
        { \
            return 0; \
        } \
        file->offset += res; \
        return res; \
    }

#define SYSFS_ENTRY_WRITE(name) \
    static int name##_write(file_t*, const char* buffer, size_t count) \
    { \
        char tmp[count + 1]; \
        memcpy(tmp, buffer, count); \
        tmp[count] = 0; \
        int value = strtoi(tmp); \
        if (value & ~1) return -EINVAL; \
        sys_config.name = value; \
        return count; \
    }

#define SYSFS_ENTRY_RDWR(name) \
    SYSFS_ENTRY_READ(name) \
    SYSFS_ENTRY_WRITE(name) \
    static inode_operations_t name##_iops; \
    static file_operations_t name##_fops = { \
        .open = &sysfs_open, \
        .read = &name##_read, \
        .write = &name##_write, \
    }

#define SYSFS_ENTRY_RDONLY(name) \
    SYSFS_ENTRY_READ(name) \
    static inode_operations_t name##_iops; \
    static file_operations_t name##_fops = { \
        .open = &sysfs_open, \
        .read = &name##_read, \
    }

sys_config_t sys_config = {
    .user_backtrace = 0,
    .segmexec = CONFIG_SEGMEXEC
};

static file_system_t sysfs = {
    .name = "sys",
    .mount = &sysfs_mount,
};

static file_operations_t sysfs_root_fops = {
    .open = &sysfs_open,
    .readdir = &sysfs_root_readdir,
};

static inode_operations_t sysfs_root_iops = {
    .lookup = &sysfs_root_lookup,
};

SYSFS_ENTRY_RDWR(user_backtrace);
SYSFS_ENTRY_RDONLY(segmexec);

static generic_vfs_entry_t root_entries[] = {
    DOT(.),
    DOT(..),
    REG(user_backtrace, S_IFREG | S_IRUGO | S_IWUGO),
    REG(segmexec, S_IFREG | S_IRUGO),
};

static int sysfs_open(file_t*)
{
    return 0;
}

static int sysfs_root_lookup(inode_t* parent, const char* name, inode_t** result)
{
    int errno;
    inode_t* new_inode;
    generic_vfs_entry_t* entry = generic_vfs_find(name, root_entries, array_size(root_entries));

    if (unlikely(!entry))
    {
        return -ENOENT;
    }

    if (unlikely(errno = inode_alloc(&new_inode)))
    {
        log_error("cannot get inode, errno %d", errno);
        return errno;
    }

    new_inode->ops = entry->iops;
    new_inode->file_ops = entry->fops;
    new_inode->ino = entry->ino;
    new_inode->sb = parent->sb;
    new_inode->mode = entry->mode;
    new_inode->fs_data = entry;

    *result = new_inode;

    return 0;
}

static int sysfs_root_readdir(file_t* file, void* buf, direntadd_t dirent_add)
{
    int i = 0;
    size_t j;
    char type;
    size_t len;
    generic_vfs_entry_t* entry;

    for (j = file->offset; j < array_size(root_entries); ++j, ++i)
    {
        type = DT_DIR;
        entry = &root_entries[j];
        len = strlen(entry->name);

        type = S_ISDIR(entry->mode) ? DT_DIR : DT_REG;

        if (dirent_add(buf, entry->name, len, 0, type))
        {
            break;
        }
    }

    file->offset = j;

    return i;
}

static int sysfs_mount(super_block_t* sb, inode_t* inode, void*, int)
{
    static super_operations_t sysfs_sb_ops;

    sb->ops = &sysfs_sb_ops;
    sb->module = 0;

    inode->ino = GENERIC_VFS_ROOT_INO;
    inode->ops = &sysfs_root_iops;
    inode->fs_data = NULL;
    inode->file_ops = &sysfs_root_fops;
    inode->size = 0;
    inode->sb = sb;

    return 0;
}

int sysfs_init(void)
{
    return file_system_register(&sysfs);
}
