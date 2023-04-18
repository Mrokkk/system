#include <kernel/fs.h>
#include <kernel/font.h>
#include <kernel/page.h>
#include <kernel/ctype.h>
#include <kernel/ksyms.h>
#include <kernel/kernel.h>
#include <kernel/module.h>

#include <arch/multiboot.h>

KERNEL_MODULE(mbfs);
module_init(mbfs_init);
module_exit(mbfs_deinit);

int mbfs_lookup(struct inode* parent, const char* name, struct inode** result);
int mbfs_open(struct file*);
int mbfs_readdir(file_t* file, void* buf, direntadd_t dirent_add);
int mbfs_mmap(struct file* file, void** data);
int mbfs_mount(super_block_t* sb, inode_t* inode, void*, int);

typedef struct mb_node
{
    char* name;
    struct module* data;
    struct inode* inode;
    struct mb_node* next;
} mb_node_t;

mb_node_t mb_root;
static ino_t ino;

static struct file_system mbfs = {
    .name = "mbfs",
    .mount = &mbfs_mount,
};

static struct super_operations mbfs_sb_ops = {
};

static struct file_operations mbfs_fops = {
    .open = &mbfs_open,
    .readdir = &mbfs_readdir,
    .mmap = &mbfs_mmap,
};

static struct inode_operations mbfs_inode_ops = {
    .lookup = &mbfs_lookup,
};

static inline void nodes_read(super_block_t* sb)
{
    int errno;
    modules_table_t* table = modules_table_get();

    mb_node_t* node = &mb_root;

    struct module* mod = (struct module*)table->modules;
    for (uint32_t i = 0; i < table->count; ++i)
    {
        inode_t* new_inode;
        mb_node_t* new_node;
        char* name = (char*)virt(mod->string);  // FIXME: why there was +1?

        log_debug(DEBUG_MBFS, "%d: %s = %x : %x",
            i,
            name,
            virt(mod->mod_start),
            virt(mod->mod_end));

        if (!strcmp(name, "kernel.map"))
        {
            ksyms_load(virt_ptr(mod->mod_start), mod->mod_end - mod->mod_start);
            ++mod;
            continue;
        }
        else if (!strcmp(name, "font.psf"))
        {
            font_load(virt_ptr(mod->mod_start), mod->mod_end - mod->mod_start);
            ++mod;
            continue;
        }

        errno = inode_get(&new_inode);

        if (errno)
        {
            log_warning("cannot get inode, errno %d", errno);
            continue;
        }

        new_inode->ino = ++ino;
        new_inode->ops = &mbfs_inode_ops;
        new_inode->file_ops = &mbfs_fops;
        new_inode->size = mod->mod_end - mod->mod_start;
        new_inode->sb = sb;

        new_node = alloc(mb_node_t);
        new_node->next = NULL;
        new_node->inode = new_inode;
        new_node->data = mod;
        new_node->name = name;
        node->next = new_node;

        new_inode->fs_data = new_node;

        ++mod;
        node = node->next;
    }
}

static inline mb_node_t* find_node(const char* name, const mb_node_t* root)
{
    mb_node_t* node = root->next;

    while (node)
    {
        if (strcmp(name, node->name) == 0)
        {
            log_debug(DEBUG_MBFS, "found node: %S", name);
            break;
        }
        node = node->next;
    }
    return node;
}

int mbfs_lookup(struct inode* parent, const char* name, struct inode** result)
{
    mb_node_t* node;
    mb_node_t* parent_node = parent->fs_data;

    if (!parent_node)
    {
        log_debug(DEBUG_MBFS, "no node for parent");
        return -ENOENT;
    }

    if (parent_node != &mb_root)
    {
        log_debug(DEBUG_MBFS, "not a root: %O", parent);
        return -ENOTDIR;
    }

    node = find_node(name, parent_node);

    if (!node)
    {
        log_debug(DEBUG_MBFS, "cannot find %S", name);
        return -ENOENT;
    }

    *result = node->inode;

    return 0;
}

int mbfs_open(struct file*)
{
    return 0;
}

int mbfs_readdir(file_t* file, void* buf, direntadd_t dirent_add)
{
    int i = 0;
    int over;
    size_t len;

    log_debug(DEBUG_MBFS, "inode=%O fs_data=%O", file->inode, file->inode->fs_data);

    mb_node_t* mb_node = file->inode->fs_data;

    if (mb_node != &mb_root)
    {
        log_debug(DEBUG_MBFS, "not a root: %O", file->inode);
        return -ENOTDIR;
    }

    mb_node = mb_node->next;

    for (; mb_node; mb_node = mb_node->next, ++i)
    {
        len = strlen(mb_node->name);

        over = dirent_add(buf, mb_node->name, len, mb_node->inode->ino, DT_REG);
        if (over)
        {
            break;
        }
    }

    return i;
}

int mbfs_mmap(struct file* file, void** data)
{
    if (!file->inode)
    {
        log_debug(DEBUG_MBFS, "inode is null");
        return -ENOENT;
    }

    mb_node_t* node = file->inode->fs_data;

    if (node == &mb_root)
    {
        return -EISDIR;
    }

    if (!node)
    {
        log_debug(DEBUG_MBFS, "fs_data is null");
        return -ENOENT;
    }

    if (!node->data)
    {
        log_debug(DEBUG_MBFS, "data is null");
    }

    *data = virt_ptr(node->data->mod_start);
    return 0;
}

int mbfs_mount(super_block_t* sb, inode_t* inode, void*, int)
{
    mb_node_t* root = &mb_root;

    sb->ops = &mbfs_sb_ops;
    sb->module = this_module;

    if (root->inode)
    {
        memcpy(inode, root->inode, sizeof(inode_t));
        return 0;
    }

    inode->ino = ++ino;
    inode->ops = &mbfs_inode_ops;
    inode->fs_data = root;
    inode->file_ops = &mbfs_fops;
    inode->size = sizeof(*root);
    inode->sb = sb;

    root->inode = inode;

    nodes_read(sb);

    return 0;
}

int mbfs_init()
{
    file_system_register(&mbfs);
    return 0;
}

int mbfs_deinit()
{
    return 0;
}
