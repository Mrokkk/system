#include <kernel/fs.h>
#include <kernel/ctype.h>
#include <kernel/device.h>

KERNEL_MODULE(devfs);
module_init(devfs_init);
module_exit(devfs_deinit);

int devfs_lookup(inode_t*, const char*, inode_t**);
int devfs_read(file_t*, char*, int);
int devfs_open(file_t*);
int devfs_readdir(file_t*, void*, direntadd_t);
int devfs_mount(super_block_t*, inode_t*, void*, int);

typedef struct dev_node
{
    char* name;
    device_t* device;
    inode_t* inode;
    struct dev_node* next;
} dev_node_t;

static ino_t ino;

dev_node_t dev_root;

static struct file_system devfs = {
    .name = "devfs",
    .mount = &devfs_mount,
};

static struct super_operations devfs_sb_ops = {
};

static struct file_operations devfs_fops = {
    .read = &devfs_read,
    .open = &devfs_open,
    .readdir = &devfs_readdir,
};

static struct inode_operations devfs_inode_ops = {
    .lookup = &devfs_lookup,
};

static inline void devices_read(super_block_t* sb)
{
    int errno;
    dev_t minor;
    inode_t* new_inode;
    device_t* chdev;
    dev_node_t* new_node;
    dev_node_t* node = &dev_root;

    for (int major = 0; major < CHAR_DEVICES_SIZE; ++major)
    {
        chdev = char_device_get(major);
        if (!chdev)
        {
            continue;
        }

        for (minor = 0; minor <= chdev->max_minor; minor++)
        {
            if (unlikely(errno = inode_get(&new_inode)))
            {
                log_error("cannot get inode, errno %d", errno);
                break;
            }

            new_inode->ops = &devfs_inode_ops;
            new_inode->file_ops = chdev->fops;
            new_inode->dev = MKDEV(major, minor);
            new_inode->ino = ++ino;
            new_inode->sb = sb;

            new_node = alloc(dev_node_t);

            if (unlikely(!new_node))
            {
                log_error("cannot allocate node for dev %u, %s", new_inode->dev, chdev->name);
                inode_put(new_inode);
                break;
            }

            new_node->next = NULL;
            new_node->inode = new_inode;
            new_node->device = chdev;
            new_node->name = slab_alloc(strlen(chdev->name) + 4); // FIXME: set size properly

            sprintf(new_node->name, "%s%u", chdev->name, minor);

            node->next = new_node;

            log_debug(DEBUG_DEVFS, "added %s", new_node->name);

            node = node->next;
        }
    }
}

int devfs_mount(super_block_t* sb, inode_t* inode, void*, int)
{
    dev_node_t* root = &dev_root;

    sb->ops = &devfs_sb_ops;
    sb->module = this_module;

    if (root->inode)
    {
        memcpy(inode, root->inode, sizeof(inode_t));
        return 0;
    }

    inode->ino = ++ino;
    inode->ops = &devfs_inode_ops;
    inode->fs_data = root;
    inode->file_ops = &devfs_fops;
    inode->size = sizeof(*root);
    inode->sb = sb;

    root->inode = inode;

    devices_read(sb);

    return 0;
}

static inline dev_node_t* find_node(const char* name, const dev_node_t* root)
{
    dev_node_t* node = root->next;

    while (node)
    {
        if (!strcmp(name, node->name))
        {
            log_debug(DEBUG_DEVFS, "found node: %S", name);
            break;
        }
        node = node->next;
    }
    return node;
}

int devfs_lookup(inode_t* dir, const char* name, inode_t** result)
{
    dev_node_t* node = dir->fs_data;

    if (!node)
    {
        log_debug(DEBUG_DEVFS, "no node for parent");
        return -ENOENT;
    }

    if (node != &dev_root)
    {
        log_debug(DEBUG_DEVFS, "not a root: %O", dir);
        return -ENOTDIR;
    }

    dev_node_t* proper_dev_node = find_node(name, node);

    if (!proper_dev_node)
    {
        log_debug(DEBUG_DEVFS, "cannot find %S", name);
        return -ENOENT;
    }

    *result = proper_dev_node->inode;

    log_debug(DEBUG_DEVFS, "finished succesfully %O", *result);

    return 0;
}

int devfs_read(file_t* file, char* buffer, int size)
{
    (void)file; (void)buffer; (void)size;
    return 0;
}

int devfs_open(file_t*)
{
    return 0;
}

int devfs_readdir(file_t* file, void* buf, direntadd_t dirent_add)
{
    int over;
    int i = 0;
    size_t len;

    log_debug(DEBUG_DEVFS, "inode=%O fs_data=%O", file->inode, file->inode->fs_data);

    dev_node_t* dev_node = file->inode->fs_data;

    if (dev_node != &dev_root)
    {
        log_debug(DEBUG_DEVFS, "not a root: %O", file->inode);
        return -ENOTDIR;
    }

    dev_node = dev_node->next;

    for (; dev_node; dev_node = dev_node->next, ++i)
    {
        len = strlen(dev_node->name);

        over = dirent_add(buf, dev_node->name, len, dev_node->inode->ino, DT_CHR);
        if (over)
        {
            break;
        }
    }

    return i;
}

int devfs_init()
{
    file_system_register(&devfs);
    return 0;
}

int devfs_deinit()
{
    return 0;
}
