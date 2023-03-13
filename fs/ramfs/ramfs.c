#include <kernel/fs.h>
#include <kernel/ctype.h>
#include <kernel/module.h>

KERNEL_MODULE(ramfs);
module_init(ramfs_init);
module_exit(ramfs_init);

struct super_block* ramfs_read_super(struct super_block*, void*, int);
void ramfs_read_inode(struct inode *);
int ramfs_lookup(struct inode*, const char*, int, struct inode**);
int ramfs_read(struct file*, char*, int);
int ramfs_write(struct file*, char*, int);
int ramfs_open(struct file *);
int ramfs_mkdir(struct inode*, const char*, int, int, inode_t** inode);
int ramfs_create(struct inode*, const char*, int, int, struct inode**);
int ramfs_readdir(struct file*, struct dirent*, int);

#define RAMFS_DIR 1
#define RAMFS_FILE 0

static ino_t ino;

typedef struct ram_node
{
    char name[256];
    char type;
    void* data;
    struct inode* inode;
} ram_node_t;

typedef struct ram_dirent
{
    ram_node_t* entry;
    struct ram_dirent* next;
} ram_dirent_t;

ram_node_t ram_root;

static struct file_system ramfs = {
    .name = "ramfs",
    .read_super = &ramfs_read_super,
};

static struct super_operations ramfs_sb_ops = {
    .read_inode = &ramfs_read_inode,
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

struct super_block* ramfs_read_super(
    struct super_block* sb,
    void* data,
    int silent)
{
    (void)sb; (void)data; (void)silent;
    sb->ops = &ramfs_sb_ops;
    return 0;
}

void ramfs_read_inode(struct inode* inode)
{
    log_debug("root inode %O", inode);
    inode->ino = ++ino;
    inode->ops = &ramfs_inode_ops;
    inode->fs_data = &ram_root;
    inode->file_ops = &ramfs_fops;
    inode->size = sizeof(ram_root);
}

ram_node_t* find_node_in_dir(const char* name, const ram_dirent_t* dirent)
{
    ram_node_t* file = NULL;
    while (dirent)
    {
        ram_node_t* temp = dirent->entry;
        if (strcmp(name, temp->name) == 0)
        {
            log_debug("found node: %S", name);
            file = temp;
            break;
        }
        dirent = dirent->next;
    }
    return file;
}

int ramfs_lookup(struct inode* dir, const char* name, int len, struct inode** result)
{
    (void)dir; (void)name; (void)len; (void)result;

    ram_node_t* ramfile = dir->fs_data;

    if (!ramfile)
    {
        log_debug("no ramfile for parent");
        return -ENOENT;
    }

    if (ramfile->type != RAMFS_DIR)
    {
        log_debug("parent not a dir: %S", ramfile->name);
        return -ENOTDIR;
    }

    ram_dirent_t* dirent = ramfile->data;

    if (!dirent)
    {
        log_debug("no dirent for parent");
        return -ENOENT;
    }

    ram_node_t* proper_ram_node = find_node_in_dir(name, dirent);

    if (!proper_ram_node)
    {
        log_debug("cannot find %S on dirent", name);
        return -ENOENT;
    }

    *result = proper_ram_node->inode;

    log_debug("finished succesfully %O", *result);

    return 0;
}

int ramfs_read(struct file *file, char *buffer, int size)
{
    ram_node_t* node = file->inode->fs_data;
    memcpy(buffer, node->data, size);
    return 0;
}

int ramfs_write(struct file *file, char *buffer, int size)
{
    ram_node_t* node = file->inode->fs_data;
    if (!node->data)
    {
        node->data = kmalloc(size * 10);
    }
    memcpy(node->data, buffer, size);
    return 0;
}

int ramfs_open(struct file* file)
{
    ram_node_t* node = file->inode->fs_data;
    (void)node;
    return 0;
}

static inline void ram_node_init(ram_node_t* this, int type, const char* name)
{
    this->type = type;
    this->data = NULL;
    this->inode = NULL;
    strcpy(this->name, name);
}

int ramfs_mkdir(struct inode* parent, const char* name, int, int, inode_t** result_inode)
{
    log_debug("parent: %O, new dir: %S", parent, name);

    ram_node_t* parent_node = parent->fs_data;
    ram_node_t* new_ram_node;

    if (!parent_node)
    {
        log_debug("no ram_node");
        return -EBADF;
    }

    if (parent_node->type != RAMFS_DIR)
    {
        log_debug("not a dir");
        return -EBADF;
    }

    new_ram_node = alloc(ram_node_t, ram_node_init(this, RAMFS_DIR, name));

    if (!parent_node->data)
    {
        log_debug("[%O]: creating first entry", parent_node);
        parent_node->data = alloc(ram_dirent_t);
        ram_dirent_t* dirent = parent_node->data;
        dirent->next = NULL;
        dirent->entry = new_ram_node;
    }
    else
    {
        ram_dirent_t* dirent = parent_node->data;

        while (dirent->next)
        {
            dirent = dirent->next;
        }

        log_debug("[%O]: creating new entry next to %s", parent_node, dirent->entry->name);
        dirent->next = alloc(ram_dirent_t);
        ram_dirent_t* new_dirent = dirent->next;
        new_dirent->next = NULL;
        new_dirent->entry = new_ram_node;
    }

    *result_inode = alloc(inode_t, inode_init(this));
    (*result_inode)->fs_data = new_ram_node;
    (*result_inode)->file_ops = &ramfs_fops;
    (*result_inode)->ops = &ramfs_inode_ops;
    (*result_inode)->size = sizeof(ram_node_t);
    (*result_inode)->ino = ++ino;
    new_ram_node->inode = *result_inode;

    return 0;
}

int ramfs_create(struct inode* parent, const char* name, int, int, struct inode** result_inode)
{
    log_debug("inode: %O, new file: %S", parent, name);

    ram_node_t* parent_node = parent->fs_data;
    ram_node_t* new_ram_node;

    if (!parent_node)
    {
        log_debug("no ram_node");
        return -EBADF;
    }

    if (parent_node->type != RAMFS_DIR)
    {
        log_debug("not a dir");
        return -EBADF;
    }

    new_ram_node = alloc(ram_node_t, ram_node_init(this, RAMFS_FILE, name));

    if (!parent_node->data)
    {
        log_debug("[%O]: creating first entry", parent_node);
        parent_node->data = alloc(ram_dirent_t);
        ram_dirent_t* dirent = parent_node->data;
        dirent->next = NULL;
        dirent->entry = new_ram_node;
    }
    else
    {
        ram_dirent_t* dirent = parent_node->data;

        while (dirent->next)
        {
            dirent = dirent->next;
        }

        log_debug("[%O]: creating new entry next to %s", parent_node, dirent->entry->name);
        dirent->next = alloc(ram_dirent_t);
        ram_dirent_t* new_dirent = dirent->next;
        new_dirent->next = NULL;
        new_dirent->entry = new_ram_node;
    }

    *result_inode = alloc(inode_t, inode_init(this));
    (*result_inode)->fs_data = new_ram_node;
    (*result_inode)->file_ops = &ramfs_fops;
    (*result_inode)->ops = &ramfs_inode_ops;
    (*result_inode)->ino = ++ino;
    (*result_inode)->size = 0;

    return 0;
}

int ramfs_readdir(struct file* file, struct dirent* dirents, int count)
{
    int i;

    log_debug("inode=%O fs_data=%O", file->inode, file->inode->fs_data);

    ram_node_t* ram_node = file->inode->fs_data;
    ram_dirent_t* ram_dirent = ram_node->data;

    for (i = 0; i < count; ++i)
    {
        if (!ram_dirent)
        {
            break;
        }
        log_debug("node: %s", ram_dirent->entry->name);
        dirents[i].ino = file->inode->ino;
        dirents[i].off = 0; // FIXME
        dirents[i].len = strlen(ram_dirent->entry->name);
        dirents[i].type = ram_dirent->entry->type; // FIXME: fs-specific type
        strcpy(dirents[i].name, ram_dirent->entry->name);
        ram_dirent = ram_dirent->next;
    }
    return i;
}

int ramfs_init()
{
    file_system_register(&ramfs);
    ram_root.type = RAMFS_DIR;
    return 0;
}
