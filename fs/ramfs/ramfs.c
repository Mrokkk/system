#include "ramfs.h"

#include <kernel/fs.h>
#include <kernel/module.h>

KERNEL_MODULE(ramfs);
module_init(ramfs_init);
module_exit(ramfs_deinit);

int ramfs_lookup(inode_t* parent, const char* name, inode_t** result);
int ramfs_read(file_t* file, char* buffer, int size);
int ramfs_write(file_t* file, const char* buffer, int count);
int ramfs_open(file_t* file);
int ramfs_mkdir(struct inode* parent, const char* name, int, int, inode_t** result);
int ramfs_create(struct inode* parent, const char* name, int, int, struct inode** result);
int ramfs_readdir(file_t* file, void* buf, direntadd_t dirent_add);
int ramfs_mount(super_block_t* sb, inode_t* inode, void*, int);

static ino_t ino;

static struct file_system ramfs = {
    .name = "ramfs",
    .mount = &ramfs_mount,
};

static struct super_operations ramfs_sb_ops = {
};

static struct file_operations ramfs_fops = {
    .read = &ramfs_read,
    .write = &ramfs_write,
    .open = &ramfs_open,
    .readdir = &ramfs_readdir,
};

static struct inode_operations ramfs_inode_ops = {
    .lookup = &ramfs_lookup,
    .mkdir = &ramfs_mkdir,
    .create = &ramfs_create
};

static inline ram_node_t* find_node_in_dir(const char* name, const ram_dirent_t* dirent)
{
    ram_node_t* temp;
    ram_node_t* file = NULL;

    while (dirent)
    {
        temp = dirent->entry;
        if (strcmp(name, temp->name) == 0)
        {
            file = temp;
            break;
        }
        dirent = dirent->next;
    }

    return file;
}

int ramfs_lookup(inode_t* parent, const char* name, inode_t** result)
{
    ram_dirent_t* dirent;
    ram_node_t* child_node;
    ram_node_t* parent_node = parent->fs_data;

    if (unlikely(!parent_node))
    {
        log_debug(DEBUG_RAMFS, "no node for parent");
        return -ENOENT;
    }

    if (unlikely(parent_node->type != DT_DIR))
    {
        log_debug(DEBUG_RAMFS, "parent not a dir: %S", parent_node->name);
        return -ENOTDIR;
    }

    if (unlikely(!(dirent = parent_node->data)))
    {
        log_debug(DEBUG_RAMFS, "no dirent for parent");
        return -ENOENT;
    }

    if (!(child_node = find_node_in_dir(name, dirent)))
    {
        log_debug(DEBUG_RAMFS, "cannot find %S on dirent", name);
        return -ENOENT;
    }

    *result = child_node->inode;

    return 0;
}

int ramfs_read(file_t* file, char* buffer, int size)
{
    ram_node_t* node = file->inode->fs_data;
    memcpy(buffer, node->data, size);
    return 0;
}

int ramfs_write(file_t* file, const char* buffer, int count)
{
    ram_node_t* node = file->inode->fs_data;
    if (!node->data)
    {
        node->data = kmalloc(count * 10);
    }
    memcpy(node->data, buffer, count);
    return 0;
}

int ramfs_open(struct file*)
{
    return 0;
}

static inline void ram_node_init(ram_node_t* this, int type, const char* name)
{
    this->type = type;
    this->data = NULL;
    this->inode = NULL;
    strcpy(this->name, name);
}

int ramfs_create_node(struct inode* parent, const char* name, int, int, inode_t** result_inode, char type)
{
    int errno;
    ram_node_t* new_node;
    ram_dirent_t* dirent;
    ram_node_t* parent_node = parent->fs_data;

    if (unlikely(!parent_node))
    {
        log_warning("parent_node is null!");
        return -EBADF;
    }

    if (unlikely(parent_node->type != DT_DIR))
    {
        return -EBADF;
    }

    new_node = alloc(ram_node_t, ram_node_init(this, type, name));

    if (unlikely(!new_node))
    {
        return -ENOMEM;
    }

    if (!parent_node->data)
    {
        parent_node->data = alloc(ram_dirent_t);
        dirent = parent_node->data;
        dirent->next = NULL;
        dirent->entry = new_node;
    }
    else
    {
        dirent = parent_node->data;

        while (dirent->next)
        {
            dirent = dirent->next;
        }

        dirent->next = alloc(ram_dirent_t);

        if (unlikely(!dirent->next))
        {
            return -ENOMEM;
        }

        ram_dirent_t* new_dirent = dirent->next;
        new_dirent->next = NULL;
        new_dirent->entry = new_node;
    }

    log_debug(DEBUG_RAMFS, "added %x:\"%s\" to parent node %x", new_node, new_node->name, parent_node);

    if (unlikely(errno = inode_get(result_inode)))
    {
        return -ENOMEM;
    }

    (*result_inode)->fs_data = new_node;
    (*result_inode)->file_ops = &ramfs_fops;
    (*result_inode)->ops = &ramfs_inode_ops;
    (*result_inode)->size = sizeof(ram_node_t);
    (*result_inode)->ino = ++ino;
    (*result_inode)->sb = parent->sb;
    new_node->inode = *result_inode;

    return 0;
}

int ramfs_mkdir(struct inode* parent, const char* name, int, int, inode_t** result)
{
    return ramfs_create_node(parent, name, 0, 0, result, DT_DIR);
}

int ramfs_create(struct inode* parent, const char* name, int, int, struct inode** result)
{
    return ramfs_create_node(parent, name, 0, 0, result, DT_REG);
}

int ramfs_readdir(file_t* file, void* buf, direntadd_t dirent_add)
{
    int i = 0;
    int over;
    size_t len;

    log_debug(DEBUG_RAMFS, "inode=%O fs_data=%O", file->inode, file->inode->fs_data);

    ram_node_t* ram_node = file->inode->fs_data;
    ram_dirent_t* ram_dirent = ram_node->data;

    for (; ram_dirent; ram_dirent = ram_dirent->next, ++i)
    {
        log_debug(DEBUG_RAMFS, "node: %s", ram_dirent->entry->name);

        len = strlen(ram_dirent->entry->name);

        over = dirent_add(buf, ram_dirent->entry->name, len, file->inode->ino, ram_dirent->entry->type);
        if (over)
        {
            break;
        }
    }

    return i;
}

int ramfs_mount(super_block_t* sb, inode_t* inode, void*, int)
{
    ram_node_t* root;

    sb->ops = &ramfs_sb_ops;
    sb->module = this_module;

    if (!(root = alloc(ram_node_t, memset(this, 0, sizeof(*this)))))
    {
        log_warning("failed to alloc root node");
        return -ENOMEM;
    }

    inode->ino = ++ino;
    inode->ops = &ramfs_inode_ops;
    inode->fs_data = root;
    inode->file_ops = &ramfs_fops;
    inode->size = sizeof(*root);
    inode->sb = sb;

    root->type = DT_DIR;
    root->inode = inode;

    return 0;
}

int ramfs_init()
{
    file_system_register(&ramfs);
    return 0;
}

int ramfs_deinit()
{
    return 0;
}
