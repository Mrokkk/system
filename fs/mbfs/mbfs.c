#include <kernel/fs.h>
#include <kernel/font.h>
#include <kernel/ctype.h>
#include <kernel/ksyms.h>
#include <kernel/kernel.h>
#include <kernel/module.h>

#include <arch/page.h>
#include <arch/multiboot.h>

KERNEL_MODULE(mbfs);
module_init(mbfs_init);
module_exit(mbfs_init);

struct super_block* mbfs_read_super(struct super_block*, void*, int);
void mbfs_read_inode(struct inode*);
int mbfs_lookup(struct inode*, const char*, int, struct inode**);
int mbfs_read(struct file*, char*, int);
int mbfs_open(struct file*);
int mbfs_readdir(struct file*, struct dirent*, int);
int mbfs_mmap(struct file*, void** data);

typedef struct mb_node
{
    char* name;
    struct module* data;
    struct inode* inode;
    struct mb_node* next;
} mb_node_t;

mb_node_t mb_root;

static struct file_system mbfs = {
    .name = "mbfs",
    .read_super = &mbfs_read_super,
};

static struct super_operations mbfs_sb_ops = {
    .read_inode = &mbfs_read_inode,
};

static struct file_operations mbfs_fops = {
    .read = &mbfs_read,
    .open = &mbfs_open,
    .readdir = &mbfs_readdir,
    .mmap = &mbfs_mmap,
};

static struct inode_operations mbfs_inode_ops = {
    .lookup = &mbfs_lookup,
};

struct super_block* mbfs_read_super(
    struct super_block* sb,
    void* data,
    int silent)
{
    (void)sb; (void)data; (void)silent;
    sb->ops = &mbfs_sb_ops;
    sb->module = this_module;
    return 0;
}

void mbfs_read_inode(struct inode* inode)
{
    log_debug("root inode %O", inode);
    inode->ops = &mbfs_inode_ops;
    inode->fs_data = &mb_root;
    inode->file_ops = &mbfs_fops;

    modules_table_t* table = modules_table_get();

    mb_node_t* node = &mb_root;
    struct module* mod = (struct module*)table->modules;
    for (uint32_t i = 0; i < table->count; ++i)
    {
        inode_t* new_inode;
        mb_node_t* new_node;
        char* name = (char*)virt(mod->string);  // FIXME: why there was +1?

        log_debug("%d: %s = %x : %x",
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

        new_inode = alloc(inode_t, inode_init(this));
        new_inode->ops = &mbfs_inode_ops;
        new_inode->file_ops = &mbfs_fops;
        new_inode->size = mod->mod_end - mod->mod_start;

        new_node = alloc(mb_node_t);
        new_node->next = NULL;
        new_node->inode = new_inode;
        new_node->data = mod;
        new_node->name = name;
        node->next = new_node;

        log_debug("%O", new_node);

        new_inode->fs_data = new_node;

        ++mod;
        node = node->next;
    }
}

mb_node_t* find_node(const char* name, const mb_node_t* root)
{
    mb_node_t* node = root->next;

    while (node)
    {
        if (strcmp(name, node->name) == 0)
        {
            log_debug("found node: %S", name);
            break;
        }
        node = node->next;
    }
    return node;
}

int mbfs_lookup(struct inode* dir, const char* name, int len, struct inode** result)
{
    (void)name; (void)len; (void)result;

    mb_node_t* mbfile = dir->fs_data;

    if (!mbfile)
    {
        log_debug("no mbfile for parent");
        return -ENOENT;
    }

    if (mbfile != &mb_root)
    {
        log_debug("not a root: %O", dir);
        return -ENOTDIR;
    }

    mb_node_t* proper_mb_node = find_node(name, mbfile);

    if (!proper_mb_node)
    {
        log_debug("cannot find %S", name);
        return -ENOENT;
    }

    *result = proper_mb_node->inode;

    log_debug("finished succesfully %O", *result);

    return 0;
}

int mbfs_read(struct file *file, char *buffer, int size)
{
    (void)file; (void)buffer; (void)size;
    return 0;
}

int mbfs_open(struct file*)
{
    return 0;
}

int mbfs_readdir(struct file* file, struct dirent* dirents, int count)
{
    int i;

    log_debug("inode=%O fs_data=%O", file->inode, file->inode->fs_data);

    mb_node_t* mb_node = file->inode->fs_data;

    if (mb_node != &mb_root)
    {
        log_debug("not a root: %O", file->inode);
        return -ENOTDIR;
    }

    mb_node = mb_node->next;

    for (i = 0; mb_node && i < count; ++i)
    {
        dirents[i].ino = mb_node->inode->ino;
        dirents[i].off = 0; // FIXME
        dirents[i].len = strlen(mb_node->name);
        dirents[i].type = 0; // FIXME
        strcpy(dirents[i].name, mb_node->name);
        mb_node = mb_node->next;
    }

    return i;
}

int mbfs_mmap(struct file* file, void** data)
{
    if (!file->inode)
    {
        log_debug("inode is null");
        return -EERR;
    }

    mb_node_t* node = file->inode->fs_data;

    if (node == &mb_root)
    {
        return -EISDIR;
    }

    if (!node)
    {
        log_debug("fs_data is null");
        return -EERR;
    }

    if (!node->data)
    {
        log_debug("data is null");
    }

    *data = virt_ptr(node->data->mod_start);
    return 0;
}

int mbfs_init()
{
    file_system_register(&mbfs);
    return 0;
}
