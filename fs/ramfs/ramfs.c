#include "ramfs.h"

#include <kernel/fs.h>
#include <kernel/list.h>
#include <kernel/minmax.h>
#include <kernel/module.h>
#include <kernel/page_alloc.h>

KERNEL_MODULE(ramfs);
module_init(ramfs_init);
module_exit(ramfs_deinit);

static int ramfs_lookup(inode_t* parent, const char* name, inode_t** result);
static int ramfs_read(file_t* file, char* buffer, size_t count);
static int ramfs_write(file_t* file, const char* buffer, size_t count);
static int ramfs_open(file_t* file);
static int ramfs_mkdir(inode_t* parent, const char* name, int, int, inode_t** result);
static int ramfs_create(inode_t* parent, const char* name, int, int, inode_t** result);
static int ramfs_readdir(file_t* file, void* buf, direntadd_t dirent_add);
static int ramfs_mount(super_block_t* sb, inode_t* inode, void*, int);

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

static page_t* ramfs_starting_page(ram_node_t* node, size_t offset)
{
    size_t page_id = offset / PAGE_SIZE;
    page_t* head = node->pages;
    page_t* current = node->pages;

    for (; page_id--;)
    {
        current = list_next_entry(&current->list_entry, page_t, list_entry);

        if (unlikely(current == head))
        {
            log_error("bug: %s(node=%x, offset=%u): fewer pages than asked offset; missing: %u",
                __func__, node, offset, page_id + 1);
            return NULL;
        }
    }

    return current;
}

typedef enum command cmd_t;

enum command
{
    TRAVERSE_CONTINUE,
    TRAVERSE_STOP,
};

typedef cmd_t (*block_cb_t)(void* data, size_t count, void* cb_data);

static int ramfs_traverse_blocks(
    ram_node_t* node,
    size_t offset,
    size_t count,
    void* cb_data,
    block_cb_t cb)
{
    size_t page_offset = offset % PAGE_SIZE;
    page_t* current = ramfs_starting_page(node, offset);
    size_t left, to_copy;

    if (unlikely(!current))
    {
        return -ESPIPE;
    }

    for (left = count = min(count, node->size - offset);
        left;
        left -= to_copy, page_offset = 0, current = list_next_entry(&current->list_entry, page_t, list_entry))
    {
        to_copy = min(PAGE_SIZE - page_offset, left);

        if (cb(shift(page_virt_ptr(current), page_offset), to_copy, cb_data) == TRAVERSE_STOP)
        {
            break;
        }
    }

    return count;
}

typedef struct lookup_context lookup_context_t;

struct lookup_context
{
    const char* name;
    ram_node_t* res;
};

static cmd_t ramfs_lookup_block(void* data, size_t count, void* cb_data)
{
    lookup_context_t* ctx = cb_data;
    ram_node_t* nodes = data;

    for (size_t i = 0; i < count / sizeof(ram_node_t); ++i)
    {
        if (!strcmp(nodes[i].name, ctx->name))
        {
            ctx->res = &nodes[i];
            return TRAVERSE_STOP;
        }
    }

    return TRAVERSE_CONTINUE;
}

static int ramfs_lookup(inode_t* parent, const char* name, inode_t** result)
{
    int res, errno;
    ram_node_t* parent_node = parent->fs_data;
    lookup_context_t ctx = {.name = name, .res = NULL};

    if (unlikely(!parent_node))
    {
        log_error("no node for parent inode: %x, ino: %u", parent, parent->ino);
        return -ENOENT;
    }

    if (unlikely(!S_ISDIR(parent_node->mode)))
    {
        log_debug(DEBUG_RAMFS, "parent not a dir: %S", parent_node->name);
        return -ENOTDIR;
    }

    res = ramfs_traverse_blocks(parent_node, 0, parent_node->size, &ctx, &ramfs_lookup_block);

    if (unlikely(errno = errno_get(res)))
    {
        return errno;
    }

    if (unlikely(!ctx.res))
    {
        return -ENOENT;
    }

    *result = ctx.res->inode;

    return 0;
}

static cmd_t ramfs_read_block(void* data, size_t count, void* cb_data)
{
    char** buffer = cb_data;
    memcpy(*buffer, data, count);
    *buffer += count;
    return TRAVERSE_CONTINUE;
}

static int ramfs_read(file_t* file, char* buffer, size_t count)
{
    int res, errno;
    size_t offset = file->offset;
    ram_node_t* node = file->dentry->inode->fs_data;

    log_debug(DEBUG_RAMFS, "reading %u B at offset %u from %s", count, offset, node->name);

    res = ramfs_traverse_blocks(node, offset, count, &buffer, &ramfs_read_block);

    if (unlikely(errno = errno_get(res)))
    {
        log_debug(DEBUG_RAMFS, "traversal failed with: %d", errno);
        return errno;
    }

    file->offset += res;

    return res;
}

static cmd_t ramfs_write_block(void* data, size_t count, void* cb_data)
{
    char** buffer = cb_data;
    memcpy(data, *buffer, count);
    *buffer += count;
    return TRAVERSE_CONTINUE;
}

static int ramfs_write(file_t* file, const char* buffer, size_t count)
{
    int res, errno;
    page_t* pages;
    size_t offset = file->offset;
    size_t page_count;
    ram_node_t* node = file->dentry->inode->fs_data;

    log_debug(DEBUG_RAMFS, "writing %u B at offset %u to %s", count, offset, node->name);

    if (offset + count > node->max_capacity)
    {
        page_count = page_align(offset + count + node->max_capacity) / PAGE_SIZE;
        pages = page_alloc(page_count, PAGE_ALLOC_DISCONT);

        if (unlikely(!pages))
        {
            log_debug(DEBUG_RAMFS, "cannot allocate %u pages for file: %s", node->name);
            return -ENOMEM;
        }

        node->max_capacity += page_count * PAGE_SIZE;
        if (node->pages)
        {
            list_merge(&node->pages->list_entry, &pages->list_entry);
            node->pages->pages_count += pages->pages_count;
        }
        else
        {
            node->pages = pages;
        }
    }

    node->size = offset + count;

    res = ramfs_traverse_blocks(node, offset, count, &buffer, &ramfs_write_block);

    if (unlikely(errno = errno_get(res)))
    {
        log_debug(DEBUG_RAMFS, "traversal failed with: %d", errno);
        return errno;
    }

    file->offset += res;
    file->dentry->inode->size = node->size;

    return res;
}

static int ramfs_open(file_t*)
{
    return 0;
}

static void ram_node_init(ram_node_t* this, mode_t mode, const char* name)
{
    this->mode = mode | S_IRUGO | S_IWUGO | S_IXUGO;
    this->pages = NULL;
    this->inode = NULL;
    this->size = 0;
    this->max_capacity = 0;
    strlcpy(this->name, name, sizeof(this->name));
}

static int ramfs_create_raw_node(inode_t* parent, const char* name, ram_node_t** result, mode_t mode)
{
    page_t* page;
    ram_node_t* new_node;
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

    if (parent_node->size + sizeof(*new_node) > parent_node->max_capacity)
    {
        if (unlikely(!(page = page_alloc(1, 0))))
        {
            return -ENOMEM;
        }

        new_node = page_virt_ptr(page);
        parent_node->max_capacity += PAGE_SIZE;

        if (parent_node->pages)
        {
            list_add_tail(&page->list_entry, &parent_node->pages->list_entry);
            parent_node->pages->pages_count += 1;
        }
        else
        {
            parent_node->pages = page;
        }
    }
    else
    {
        page = ramfs_starting_page(parent_node, parent_node->size);

        if (unlikely(!page))
        {
            return -EINVAL;
        }

        new_node = shift_as(ram_node_t*, page_virt_ptr(page), parent_node->size % PAGE_SIZE);
    }

    parent_node->size += sizeof(*new_node);
    parent->size = parent_node->size;

    ram_node_init(new_node, mode, name);

    log_debug(DEBUG_RAMFS, "added %x:\"%s\" to parent node %x", new_node, new_node->name, parent_node);

    *result = new_node;
    return 0;
}

static int ramfs_create_node(
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

    if (unlikely(errno = inode_alloc(result_inode)))
    {
        return errno;
    }

    (*result_inode)->fs_data = new_node;
    (*result_inode)->file_ops = &ramfs_fops;
    (*result_inode)->ops = &ramfs_inode_ops;
    (*result_inode)->size = sizeof(ram_node_t);
    (*result_inode)->ino = ++sb->last_ino;
    (*result_inode)->mode = new_node->mode;
    new_node->inode = *result_inode;

    return 0;
}

static int ramfs_mkdir(inode_t* parent, const char* name, int, int, inode_t** result)
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

static int ramfs_create(inode_t* parent, const char* name, int, int, inode_t** result)
{
    return ramfs_create_node(parent, name, result, S_IFREG);
}

static void ram_sb_init(ram_sb_t* sb, ram_node_t* root)
{
    sb->root = root;
    sb->last_ino = 1;
}

typedef struct readdir_context readdir_context_t;

struct readdir_context
{
    direntadd_t dirent_add;
    void*       buf;
    size_t      count;
};

static cmd_t ramfs_readdir_block(void* data, size_t count, void* cb_data)
{
    int over;
    readdir_context_t* ctx = cb_data;
    ram_node_t* nodes = data;

    for (size_t i = 0; i < count / sizeof(ram_node_t); ++i)
    {
        over = ctx->dirent_add(
            ctx->buf,
            nodes[i].name,
            strlen(nodes[i].name),
            nodes[i].inode ? nodes[i].inode->ino : 0,
            mode_to_type(nodes[i].mode));

        if (over)
        {
            return TRAVERSE_STOP;
        }

        ctx->count++;
    }

    return TRAVERSE_CONTINUE;
}

static int ramfs_readdir(file_t* file, void* buf, direntadd_t dirent_add)
{
    int res, errno;
    ram_node_t* parent = file->dentry->inode->fs_data;

    readdir_context_t ctx = {.dirent_add = dirent_add, .buf = buf, .count = 0};

    res = ramfs_traverse_blocks(parent, 0, parent->size, &ctx, &ramfs_readdir_block);

    if (unlikely(errno = errno_get(res)))
    {
        return errno;
    }

    return ctx.count;
}

static int ramfs_mount(super_block_t* sb, inode_t* inode, void*, int)
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
