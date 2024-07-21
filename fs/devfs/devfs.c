#define log_fmt(fmt) "devfs: " fmt
#include <kernel/fs.h>
#include <kernel/devfs.h>

static int devfs_lookup(inode_t* dir, const char* name, inode_t** result);
static int devfs_open(file_t* file);
static int devfs_readdir(file_t* file, void* buf, direntadd_t dirent_add);
static int devfs_mount(super_block_t* sb, inode_t* inode, void*, int);

typedef struct
{
    dev_t major, minor;
    file_operations_t* ops;
} device_node_t;

typedef struct
{
    struct dev_inode* first;
    struct dev_inode* last;
} dir_node_t;

typedef struct
{
    mode_t mode;
    union
    {
        dir_node_t dir;
        device_node_t dev;
    };
} dev_data_t;

typedef struct dev_inode
{
    ino_t ino;
    char* name;
    struct dev_inode* next;
    struct dev_inode* prev;
    dev_data_t* data;
} dev_inode_t;

static ino_t ino;
static dev_inode_t* dev_root;

static file_system_t devfs = {
    .name = "devfs",
    .mount = &devfs_mount,
};

static super_operations_t devfs_sb_ops = {
};

static file_operations_t devfs_fops = {
    .open = &devfs_open,
    .readdir = &devfs_readdir,
};

static inode_operations_t devfs_inode_ops = {
    .lookup = &devfs_lookup,
};

static inline dev_inode_t* dev_inode_alloc()
{
    dev_inode_t* node = slab_alloc(sizeof(dev_inode_t) + sizeof(dev_data_t));

    if (unlikely(!node))
    {
        return node;
    }

    node->data = ptr(addr(node) + sizeof(*node));
    node->next = NULL;
    node->prev = NULL;
    return node;
}

static inline dev_inode_t* dev_empty_inode_alloc(char* name)
{
    dev_inode_t* node = slab_alloc(sizeof(dev_inode_t));

    if (unlikely(!node))
    {
        return node;
    }

    memset(node, 0, sizeof(*node));
    node->name = name;
    return node;
}

static inline void dev_add_to_dir(dir_node_t* dir, dev_inode_t* node)
{
    if (!dir->last)
    {
        dir->first = node;
        dir->last = node;
        node->next = NULL;
        node->prev = NULL;
    }
    else
    {
        dir->last->next = node;
        node->prev = dir->last;
        dir->last = node;
        node->next = NULL;
    }
}

UNMAP_AFTER_INIT int devfs_init()
{
    dev_inode_t* dot;
    dev_inode_t* ddot;

    dev_root = dev_inode_alloc();

    if (unlikely(!dev_root))
    {
        log_error("cannot allocate root node");
        return -ENOMEM;
    }

    dev_root->ino = ++ino;
    dev_root->data->dir.first = NULL;
    dev_root->data->dir.last = NULL;
    dev_root->data->mode = S_IFDIR | S_IRUGO | S_IWUGO;

    dot = dev_empty_inode_alloc(".");
    ddot = dev_empty_inode_alloc("..");

    dev_add_to_dir(&dev_root->data->dir, dot);
    dev_add_to_dir(&dev_root->data->dir, ddot);

    file_system_register(&devfs);
    return 0;
}

int devfs_register(const char* name, dev_t major, dev_t minor, file_operations_t* fops)
{
    dev_inode_t* new_node;

    if (unlikely(!(new_node = dev_inode_alloc())))
    {
        log_error("cannot allocate node for dev %u:%u", major, minor);
        return -ENOMEM;
    }

    new_node->ino = ++ino;
    new_node->name = slab_alloc(strlen(name) + 1);
    new_node->data->mode = S_IFCHR | S_IRUGO | S_IWUGO;
    new_node->data->dev.major = major;
    new_node->data->dev.minor = minor;
    new_node->data->dev.ops = fops;
    strcpy(new_node->name, name);

    dev_add_to_dir(&dev_root->data->dir, new_node);

    log_debug(DEBUG_DEVFS, "added %s", new_node->name);

    return 0;
}

int devfs_blk_register(const char* name, dev_t major, dev_t minor, file_operations_t* ops)
{
    dev_inode_t* new_node;

    if (unlikely(!(new_node = dev_inode_alloc())))
    {
        log_error("cannot allocate node for dev %u:%u", major, minor);
        return -ENOMEM;
    }

    new_node->ino = ++ino;
    new_node->name = slab_alloc(strlen(name) + 1);
    new_node->data->mode = S_IFBLK | S_IRUGO | S_IWUGO;
    new_node->data->dev.major = major;
    new_node->data->dev.minor = minor;
    new_node->data->dev.ops = ops;
    strcpy(new_node->name, name);

    dev_add_to_dir(&dev_root->data->dir, new_node);

    log_debug(DEBUG_DEVFS, "added %s", new_node->name);

    return 0;
}

static int devfs_mount(super_block_t* sb, inode_t* inode, void*, int)
{
    sb->ops = &devfs_sb_ops;
    sb->module = 0;

    inode->ino = dev_root->ino;
    inode->ops = &devfs_inode_ops;
    inode->fs_data = dev_root;
    inode->file_ops = &devfs_fops;
    inode->size = sizeof(*root);
    inode->sb = sb;

    return 0;
}

static inline dev_inode_t* node_find(const char* name)
{
    for (dev_inode_t* node = dev_root->data->dir.first; node; node = node->next)
    {
        if (!strcmp(name, node->name))
        {
            log_debug(DEBUG_DEVFS, "found node: %S", name);
            return node;
        }
    }
    return NULL;
}

struct file_operations* devfs_blk_get(const char* name, dev_t* dev)
{
    dev_inode_t* node = node_find(name);
    device_node_t* device;

    if (!node)
    {
        return NULL;
    }

    device = &node->data->dev;
    *dev = MKDEV(device->major, device->minor);

    return device->ops;
}

static int devfs_lookup(inode_t* dir, const char* name, inode_t** result)
{
    int errno;
    inode_t* new_inode;
    dev_inode_t* dir_node = dir->fs_data;
    dev_inode_t* child_node;
    device_node_t* device;

    if (!dir_node)
    {
        log_debug(DEBUG_DEVFS, "no node for parent");
        return -ENOENT;
    }

    if (dir_node != dev_root)
    {
        log_debug(DEBUG_DEVFS, "not a root: %O", dir);
        return -ENOTDIR;
    }

    child_node = node_find(name);

    if (!child_node)
    {
        log_debug(DEBUG_DEVFS, "cannot find %S", name);
        return -ENOENT;
    }

    if (unlikely(errno = inode_get(&new_inode)))
    {
        log_error("cannot get inode, errno %d", errno);
        return errno;
    }

    device = &child_node->data->dev;

    new_inode->ops      = &devfs_inode_ops;
    new_inode->file_ops = device->ops;
    new_inode->dev      = dir->dev;
    new_inode->rdev     = MKDEV(device->major, device->minor);
    new_inode->ino      = ++ino;
    new_inode->sb       = dir->sb;
    new_inode->mode     = child_node->data->mode;

    *result = new_inode;

    log_debug(DEBUG_DEVFS, "finished succesfully %O", *result);

    return 0;
}

static int devfs_open(file_t*)
{
    return 0;
}

static int devfs_readdir(file_t* file, void* buf, direntadd_t dirent_add)
{
    int i = 0;
    char type;
    size_t len;
    dev_inode_t* node;
    dir_node_t* dir;
    dev_data_t* data;

    log_debug(DEBUG_DEVFS, "inode=%O fs_data=%O", file->inode, file->inode->fs_data);

    node = file->inode->fs_data;

    if (node != dev_root)
    {
        log_debug(DEBUG_DEVFS, "not a root: %O", file->inode);
        return -ENOTDIR;
    }

    dir = &node->data->dir;

    for (node = dir->first; node; node = node->next, ++i)
    {
        type = DT_DIR;
        data = node->data;
        len = strlen(node->name);

        if (data)
        {
            type = mode_to_type(data->mode);
        }

        if (dirent_add(buf, node->name, len, node->ino, type))
        {
            break;
        }
    }

    return i;
}
