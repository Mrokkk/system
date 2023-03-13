#include <kernel/fs.h>
#include <kernel/ctype.h>
#include <kernel/device.h>

KERNEL_MODULE(devfs);
module_init(devfs_init);
module_exit(devfs_init);

static ino_t ino;

struct super_block* devfs_read_super(struct super_block*, void*, int);
void devfs_read_inode(struct inode*);
int devfs_lookup(struct inode*, const char*, int, struct inode**);
int devfs_read(struct file*, char*, int);
int devfs_open(struct file*);
int devfs_readdir(struct file*, struct dirent*, int);

typedef struct dev_node
{
    char* name;
    device_t* device;
    inode_t* inode;
    struct dev_node* next;
} dev_node_t;

dev_node_t dev_root;

static struct file_system devfs = {
    .name = "devfs",
    .read_super = &devfs_read_super,
};

static struct super_operations devfs_sb_ops = {
    .read_inode = &devfs_read_inode,
};

static struct file_operations devfs_fops = {
    .read = &devfs_read,
    .open = &devfs_open,
    .readdir = &devfs_readdir,
};

static struct inode_operations devfs_inode_ops = {
    .lookup = &devfs_lookup,
};

struct super_block* devfs_read_super(
    struct super_block* sb,
    void* data,
    int silent)
{
    (void)sb; (void)data; (void)silent;
    sb->ops = &devfs_sb_ops;
    return 0;
}

void devfs_read_inode(struct inode* inode)
{
    dev_t minor;
    inode_t* new_inode;
    dev_node_t* new_node;
    dev_node_t* node = &dev_root;

    inode->ops = &devfs_inode_ops;
    inode->fs_data = &dev_root;
    inode->file_ops = &devfs_fops;

    for (int major = 0; major < CHAR_DEVICES_SIZE; ++major)
    {
        if (!char_devices[major])
        {
            continue;
        }

        for (minor = 0; minor <= char_devices[major]->max_minor; minor++)
        {
            new_inode = alloc(inode_t, inode_init(this));
            new_inode->ops = &devfs_inode_ops;
            new_inode->file_ops = char_devices[major]->fops;
            new_inode->dev = MKDEV(major, minor);
            new_inode->ino = ++ino;

            new_node = alloc(dev_node_t);
            new_node->next = NULL;
            new_node->inode = new_inode;
            new_node->device = char_devices[major];
            new_node->name = kmalloc(strlen(char_devices[major]->name) + 4); // FIXME: set size properly

            sprintf(new_node->name, "%s%u", char_devices[major]->name, minor);

            node->next = new_node;

            log_debug("added %s", new_node->name);

            node = node->next;
        }
    }
}

static inline dev_node_t* find_node(const char* name, const dev_node_t* root)
{
    dev_node_t* node = root->next;

    while (node)
    {
        if (!strcmp(name, node->name))
        {
            log_debug("found node: %S", name);
            break;
        }
        node = node->next;
    }
    return node;
}

int devfs_lookup(struct inode* dir, const char* name, int len, struct inode** result)
{
    (void)len; (void)result;

    dev_node_t* node = dir->fs_data;

    if (!node)
    {
        log_debug("no node for parent");
        return -ENOENT;
    }

    if (node != &dev_root)
    {
        log_debug("not a root: %O", dir);
        return -ENOTDIR;
    }

    dev_node_t* proper_dev_node = find_node(name, node);

    if (!proper_dev_node)
    {
        log_debug("cannot find %S", name);
        return -ENOENT;
    }

    *result = proper_dev_node->inode;

    log_debug("finished succesfully %O", *result);

    return 0;
}

int devfs_read(struct file *file, char *buffer, int size)
{
    (void)file; (void)buffer; (void)size;
    return 0;
}

int devfs_open(struct file*)
{
    return 0;
}

int devfs_readdir(struct file* file, struct dirent* dirents, int count)
{
    int i;

    log_debug("inode=%O fs_data=%O", file->inode, file->inode->fs_data);

    dev_node_t* dev_node = file->inode->fs_data;

    if (dev_node != &dev_root)
    {
        log_debug("not a root: %O", file->inode);
        return -ENOTDIR;
    }

    dev_node = dev_node->next;

    for (i = 0; dev_node && i < count; ++i)
    {
        dirents[i].ino = dev_node->inode->ino;
        dirents[i].off = 0; // FIXME
        dirents[i].len = strlen(dev_node->name);
        dirents[i].type = 0; // FIXME
        strcpy(dirents[i].name, dev_node->name);
        dev_node = dev_node->next;
    }

    return i;
}

int devfs_init()
{
    file_system_register(&devfs);
    return 0;
}
