#include "ramfs.h"

#include <kernel/fs.h>
#include <kernel/page.h>
#include <kernel/module.h>

KERNEL_MODULE(ramfs);
module_init(ramfs_init);
module_exit(ramfs_deinit);

int ramfs_lookup(inode_t* parent, const char* name, inode_t** result);
int ramfs_read(file_t* file, char* buffer, size_t count);
int ramfs_write(file_t* file, const char* buffer, size_t count);
int ramfs_open(file_t* file);
int ramfs_mkdir(inode_t* parent, const char* name, int, int, inode_t** result);
int ramfs_create(inode_t* parent, const char* name, int, int, inode_t** result);
int ramfs_readdir(file_t* file, void* buf, direntadd_t dirent_add);
int ramfs_mount(super_block_t* sb, inode_t* inode, void*, int);

static file_system_t ramfs = {
    .name = "ramfs",
    .mount = &ramfs_mount,
};

static super_operations_t ramfs_sb_ops;

static file_operations_t ramfs_fops = {
    .read = &ramfs_read,
    .write = &ramfs_write,
    .open = &ramfs_open,
    .readdir = &ramfs_readdir,
};

static inode_operations_t ramfs_inode_ops = {
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

    if (unlikely(!S_ISDIR(parent_node->mode)))
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

int ramfs_read(file_t* file, char* buffer, size_t count)
{
    ram_node_t* node = file->inode->fs_data;
    memcpy(buffer, node->data, count);
    file->offset += count;
    return count;
}

int ramfs_write(file_t* file, const char* buffer, size_t count)
{
    page_t* page;
    ram_node_t* node = file->inode->fs_data;
    if (!node->data)
    {
        page = page_alloc1();
        if (!page)
        {
            return -ENOMEM;
        }
        node->data = page_virt_ptr(page);
    }
    memcpy(node->data, buffer, count);
    file->offset += count;
    return count;
}

int ramfs_open(file_t*)
{
    return 0;
}

static inline void ram_node_init(ram_node_t* this, mode_t mode, const char* name)
{
    this->mode = mode | S_IRUGO | S_IWUGO | S_IXUGO;
    this->data = NULL;
    this->inode = NULL;
    strcpy(this->name, name);
}

static inline int ramfs_create_raw_node(inode_t* parent, const char* name, ram_node_t** result, mode_t mode)
{
    ram_node_t* new_node;
    ram_dirent_t* dirent;
    ram_node_t* parent_node = parent->fs_data;

    if (unlikely(!parent_node))
    {
        log_warning("parent_node is null!");
        return -EBADF;
    }

    if (unlikely(!S_ISDIR(parent_node->mode)))
    {
        return -EBADF;
    }

    new_node = alloc(ram_node_t, ram_node_init(this, mode, name));

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

    *result = new_node;
    return 0;
}

int ramfs_create_node(
    inode_t* parent,
    const char* name,
    inode_t** result_inode,
    mode_t mode)
{
    int errno;
    ram_node_t* new_node;
    ram_sb_t* sb = parent->sb->fs_data;
    size_t len = strlen(name);

    if (unlikely(len >= RAMFS_NAME_MAX_LEN))
    {
        return -ENAMETOOLONG;
    }

    if (unlikely(errno = ramfs_create_raw_node(parent, name, &new_node, mode)))
    {
        return errno;
    }

    if (unlikely(errno = inode_get(result_inode)))
    {
        return errno;
    }

    (*result_inode)->fs_data = new_node;
    (*result_inode)->file_ops = &ramfs_fops;
    (*result_inode)->ops = &ramfs_inode_ops;
    (*result_inode)->size = sizeof(ram_node_t);
    (*result_inode)->ino = ++sb->last_ino;
    (*result_inode)->sb = parent->sb;
    (*result_inode)->mode = new_node->mode;
    new_node->inode = *result_inode;

    return 0;
}

int ramfs_mkdir(inode_t* parent, const char* name, int, int, inode_t** result)
{
    int errno;
    ram_node_t* dot;
    ram_node_t* ddot;

    if ((errno = ramfs_create_node(parent, name, result, S_IFDIR)))
    {
        return errno;
    }

    if ((errno = ramfs_create_raw_node(*result, ".", &dot, S_IFDIR)))
    {
        goto free_parent;
    }

    if ((errno = ramfs_create_raw_node(*result, "..", &ddot, S_IFDIR)))
    {
        goto free_dot;
    }

    dot->inode = *result;
    ddot->inode = parent;

    return 0;

free_dot:
    delete(dot);
free_parent:
    delete(*result);
    return errno;
}

int ramfs_create(inode_t* parent, const char* name, int, int, inode_t** result)
{
    return ramfs_create_node(parent, name, result, S_IFREG);
}

int ramfs_readdir(file_t* file, void* buf, direntadd_t dirent_add)
{
    int i = 0;
    int over;
    size_t len;
    char type;

    log_debug(DEBUG_RAMFS, "inode=%O fs_data=%O", file->inode, file->inode->fs_data);

    ram_node_t* ram_node = file->inode->fs_data;
    ram_dirent_t* ram_dirent = ram_node->data;

    for (; ram_dirent; ram_dirent = ram_dirent->next, ++i)
    {
        log_debug(DEBUG_RAMFS, "node: %s", ram_dirent->entry->name);

        type = mode_to_type(ram_dirent->entry->mode);
        len = strlen(ram_dirent->entry->name);

        over = dirent_add(buf, ram_dirent->entry->name, len, file->inode->ino, type);
        if (over)
        {
            break;
        }
    }

    return i;
}

static inline void ram_sb_init(ram_sb_t* sb, ram_node_t* root)
{
    sb->root = root;
    sb->last_ino = 1;
}

int ramfs_mount(super_block_t* sb, inode_t* inode, void*, int)
{
    int errno;
    ram_node_t* root;
    ram_node_t* dot;
    ram_node_t* ddot;
    ram_sb_t* ram_sb;

    sb->ops = &ramfs_sb_ops;
    sb->module = this_module;

    if (!(root = alloc(ram_node_t, memset(this, 0, sizeof(*this)))))
    {
        log_warning("failed to alloc root node");
        return -ENOMEM;
    }

    if (!(ram_sb = alloc(ram_sb_t, ram_sb_init(this, root))))
    {
        log_warning("failed to alloc superblock");
        return -ENOMEM;
    }

    sb->fs_data = ram_sb;
    inode->ops = &ramfs_inode_ops;
    inode->fs_data = root;
    inode->file_ops = &ramfs_fops;
    inode->size = sizeof(*root);
    inode->sb = sb;

    root->mode = inode->mode;
    root->inode = inode;

    if ((errno = ramfs_create_raw_node(inode, ".", &dot, S_IFDIR)))
    {
        log_warning("cannot add \".\" to root");
        return errno;
    }

    dot->inode = inode;

    if ((errno = ramfs_create_raw_node(inode, "..", &ddot, S_IFDIR)))
    {
        log_warning("cannot add \"..\" to root");
        return errno;
    }

    ddot->inode = NULL;

    return 0;
}

UNMAP_AFTER_INIT int ramfs_init()
{
    return file_system_register(&ramfs);
}

int ramfs_deinit()
{
    return 0;
}
