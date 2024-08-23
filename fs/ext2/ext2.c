#define log_fmt(fmt) "ext2: " fmt
#include "ext2.h"

#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/ctype.h>
#include <kernel/kernel.h>
#include <kernel/minmax.h>
#include <kernel/module.h>
#include <kernel/process.h>
#include <kernel/api/dirent.h>

KERNEL_MODULE(ext2);
module_init(ext2_init);
module_exit(ext2_deinit);

static int ext2_lookup(inode_t* parent, const char* name, inode_t** result);
static int ext2_open(file_t* file);
static int ext2_readdir(file_t* file, void* buf, direntadd_t dirent_add);
static int ext2_mmap(file_t* file, vm_area_t* vma);
static int ext2_mount(super_block_t* sb, inode_t* inode, void*, int);
static int ext2_read(file_t* file, char* buffer, size_t count);
static int ext2_nopage(vm_area_t* vma, uintptr_t address, page_t** page);
static int ext2_readlink(inode_t* inode, char* buffer, size_t size);

enum traverse_command
{
    TRAVERSE_CONTINUE,
    TRAVERSE_STOP,
};

typedef enum traverse_command cmd_t;

typedef struct level level_t;
typedef cmd_t (*block_cb_t)(void*, size_t, void*);

static int ext2_traverse_blocks(
    ext2_data_t* data,
    ext2_inode_t* raw_inode,
    size_t offset,
    size_t count,
    void* cb_data,
    block_cb_t cb);

// Each block can be addressed using leve[4], each for each
// level of indirection

#define LVL_DIRECT              0
#define LVL_SINGLY_INDIRECT     1
#define LVL_DOUBLY_INDIRECT     2
#define LVL_TRIPLY_INDIRECT     3

struct level
{
    int         lvl;
    uint32_t    cur;     // list element index
    uint32_t*   cur_ptr; // ptr to list element
};

static file_system_t ext2 = {
    .name  = "ext2",
    .mount = &ext2_mount,
};

static super_operations_t ext2_sb_ops = {
};

static file_operations_t ext2_fops = {
    .open    = &ext2_open,
    .read    = &ext2_read,
    .readdir = &ext2_readdir,
    .mmap    = &ext2_mmap,
};

static inode_operations_t ext2_inode_ops = {
    .lookup = &ext2_lookup,
    .readlink = &ext2_readlink,
};

static vm_operations_t ext2_vmops = {
    .nopage = &ext2_nopage,
};

static ext2_bgd_t* ext2_bgd_get(ext2_data_t* data, uint32_t ino)
{
    buffer_t* b;
    ext2_bgd_t* bgd;
    uint32_t group_index = (ino - 1) / data->inodes_per_group;
    uint32_t group_block_nr = (group_index * sizeof(ext2_bgd_t)) / EXT2_BLOCK_SIZE + 2;

    group_index %= EXT2_BLOCK_SIZE / sizeof(ext2_bgd_t);

    b = block_read(data->dev, data->file, group_block_nr);

    if (unlikely(errno_get(b)))
    {
        return NULL;
    }

    return (bgd = b->data) + group_index;
}

static ext2_inode_t* ext2_inode_get(ext2_data_t* data, uint32_t ino)
{
    buffer_t* b;
    ext2_inode_t* inode;
    uint32_t inode_in_group_index = (ino - 1) % data->inodes_per_group;
    uint32_t block_nr = inode_in_group_index / data->inodes_per_block;
    uint32_t block_off = inode_in_group_index % data->inodes_per_block;

    ext2_bgd_t* bgd = ext2_bgd_get(data, ino);

    if (unlikely(!bgd))
    {
        return NULL;
    }

    b = block_read(data->dev, data->file, bgd->inode_table + block_nr);

    if (unlikely(errno_get(b)))
    {
        return NULL;
    }

    return (inode = b->data) + block_off;
}

typedef struct lookup_context lookup_context_t;

struct lookup_context
{
    ext2_data_t* data;
    const char* name;
    size_t name_len;
    ext2_inode_t* res;
    ino_t ino;
};

static cmd_t ext2_lookup_block(void* block, size_t to_copy, void* data)
{
    lookup_context_t* ctx = data;
    ext2_dir_entry_t* dirent;
    size_t total_len;

    for (dirent = block, total_len = 0;
        total_len < to_copy;
        total_len += dirent->rec_len, dirent = ptr(addr(dirent) + dirent->rec_len))
    {
        if (ctx->name_len != dirent->name_len || strncmp(dirent->name, ctx->name, dirent->name_len))
        {
            continue;
        }

        ctx->res = ext2_inode_get(ctx->data, dirent->inode);

        if (unlikely(!ctx->res))
        {
            log_debug(DEBUG_EXT2FS, "cannot read inode %u", dirent->inode);
            return -EIO;
        }

        ctx->ino = dirent->inode;

        log_debug(DEBUG_EXT2FS, "found inode; ino=%u, size=%u, name=%S",
            dirent->inode,
            ctx->res->size,
            dirent->name);

        return TRAVERSE_STOP;
    }

    return TRAVERSE_CONTINUE;
}

static int ext2_lookup_raw(
    ext2_data_t* data,
    ext2_inode_t* parent_inode,
    const char* name,
    ext2_inode_t** child_inode,
    ino_t* ino)
{
    int res, errno;
    lookup_context_t ctx = {.data = data, .name = name, .name_len = strlen(name), .ino = 0, .res = NULL};

    log_debug(DEBUG_EXT2FS, "name=%s", name);

    res = ext2_traverse_blocks(data, parent_inode, 0, parent_inode->size, &ctx, &ext2_lookup_block);

    if (unlikely(errno = errno_get(res)))
    {
        return errno;
    }

    if (unlikely(!ctx.res))
    {
        log_debug(DEBUG_EXT2FS, "no inode with name = %s", name);
        return -ENOENT;
    }

    *child_inode = ctx.res;
    *ino = ctx.ino;

    return 0;
}

static int ext2_lookup(inode_t* parent, const char* name, inode_t** result)
{
    int errno;
    ino_t ino;
    ext2_inode_t* child_inode;
    ext2_inode_t* parent_inode = parent->fs_data;
    ext2_data_t* data = parent->sb->fs_data;

    if (unlikely(parent->ino == EXT2_LOST_FOUND_INO))
    {
        return -ENOENT;
    }

    if (unlikely(errno = ext2_lookup_raw(data, parent_inode, name, &child_inode, &ino)))
    {
        return errno;
    }

    if (unlikely(errno = inode_alloc(result)))
    {
        return errno;
    }

    (*result)->ino = ino;
    (*result)->ops = &ext2_inode_ops;
    (*result)->sb = parent->sb;
    (*result)->size = child_inode->size;
    (*result)->file_ops = &ext2_fops;
    (*result)->fs_data = child_inode;
    (*result)->mode = child_inode->mode;
    (*result)->uid = child_inode->uid;
    (*result)->gid = child_inode->gid;
    (*result)->ctime = child_inode->ctime;
    (*result)->mtime = child_inode->mtime;
    (*result)->nlink = child_inode->links_count;

    return 0;
}

static int ext2_open(file_t*)
{
    return 0;
}

static int level_escalate(ext2_data_t* data, level_t** level)
{
    buffer_t* b;
    level_t* cur = *level;

    b = block_read(data->dev, data->file, *cur->cur_ptr);

    if (unlikely(errno_get(b)))
    {
        return errno_get(b);
    }

    ++cur;
    cur->cur = 0;
    cur->cur_ptr = b->data;
    *level = cur;

    return 0;
}

static int prev_level_forward(ext2_data_t* data, level_t* cur_lvl)
{
    buffer_t* b;
    level_t* prev_lvl = cur_lvl - 1;

    ++prev_lvl->cur;
    ++prev_lvl->cur_ptr;

    b = block_read(data->dev, data->file, *prev_lvl->cur_ptr);

    if (unlikely(errno_get(b)))
    {
        return errno_get(b);
    }

    cur_lvl->cur = 0;
    cur_lvl->cur_ptr = b->data;

    return 0;
}

static int ext2_next_block(ext2_data_t* data, level_t** cur_lvl, buffer_t** cur_block)
{
    int errno;
    buffer_t* b;
    level_t* cur = *cur_lvl;

    ++cur->cur;
    ++cur->cur_ptr;

    switch (cur->lvl)
    {
        case LVL_DIRECT:
            if (cur->cur == 12)
            {
                if ((errno = level_escalate(data, &cur)))
                {
                    return errno;
                }
            }
            break;
        case LVL_SINGLY_INDIRECT:
            if (cur->cur == 256)
            {
                if ((errno = prev_level_forward(data, cur)) ||
                    (errno = level_escalate(data, &cur)))
                {
                    return errno;
                }
            }
            break;
        case LVL_DOUBLY_INDIRECT:
            if (cur->cur == 256)
            {
                if ((cur - 1)->cur == 255)
                {
                    log_info("triply-indirect addressing not yet supported");
                    return -EFBIG;
                }

                if ((errno = prev_level_forward(data, cur)))
                {
                    return errno;
                }
            }
    }

    *cur_lvl = cur;

    b = block_read(data->dev, data->file, *cur->cur_ptr);

    if (unlikely((errno = errno_get(b))))
    {
        return errno;
    }

    *cur_block = b;

    return 0;
}

#define NDIR_BLOCKS     (12)
#define IND_BLOCKS      (EXT2_BLOCK_SIZE / sizeof(uint32_t))
#define DIND_BLOCKS     (IND_BLOCKS * IND_BLOCKS)

static int ext2_traverse_blocks_init(
    ext2_data_t* data,
    ext2_inode_t* raw_inode,
    uint32_t blocknr,
    level_t* levels,
    level_t** cur,
    buffer_t** cur_block)
{
    buffer_t* b;

    levels[0].lvl = 0;
    levels[1].lvl = 1;
    levels[2].lvl = 2;
    levels[3].lvl = 3;

    if (blocknr >= NDIR_BLOCKS + IND_BLOCKS + DIND_BLOCKS)
    {
        log_info("failed to setup; triply-indirect addressing not yet supported");
        return -EFBIG;
    }
    else if (blocknr >= NDIR_BLOCKS + IND_BLOCKS) // doubly-indirect addressing
    {
        *cur = levels + 2;
        blocknr -= NDIR_BLOCKS + IND_BLOCKS;

        b = block_read(data->dev, data->file, raw_inode->block[EXT2_DIND_BLOCK]);

        if (unlikely(errno_get(b)))
        {
            return errno_get(b);
        }

        levels[LVL_DIRECT].cur = EXT2_DIND_BLOCK;
        levels[LVL_DIRECT].cur_ptr = raw_inode->block + EXT2_DIND_BLOCK;

        levels[LVL_SINGLY_INDIRECT].cur = blocknr / IND_BLOCKS;
        levels[LVL_SINGLY_INDIRECT].cur_ptr = (uint32_t*)b->data + blocknr / IND_BLOCKS;

        b = block_read(data->dev, data->file, *levels[1].cur_ptr);

        if (unlikely(errno_get(b)))
        {
            return errno_get(b);
        }

        levels[LVL_DOUBLY_INDIRECT].cur = blocknr % IND_BLOCKS;
        levels[LVL_DOUBLY_INDIRECT].cur_ptr = (uint32_t*)b->data + blocknr % IND_BLOCKS;
    }
    else if (blocknr >= NDIR_BLOCKS) // indirect addressing
    {
        *cur = levels + 1;
        blocknr -= NDIR_BLOCKS;

        b = block_read(data->dev, data->file, raw_inode->block[EXT2_NDIR_BLOCKS]);

        if (unlikely(errno_get(b)))
        {
            return errno_get(b);
        }

        levels[LVL_DIRECT].cur = EXT2_NDIR_BLOCKS;
        levels[LVL_DIRECT].cur_ptr = raw_inode->block + EXT2_NDIR_BLOCKS;

        levels[LVL_SINGLY_INDIRECT].cur = blocknr;
        levels[LVL_SINGLY_INDIRECT].cur_ptr = (uint32_t*)b->data + blocknr;
    }
    else // direct addressing
    {
        *cur = levels;
        levels[LVL_DIRECT].cur = blocknr;
        levels[LVL_DIRECT].cur_ptr = raw_inode->block + blocknr;
    }

    b = block_read(data->dev, data->file, *(*cur)->cur_ptr);

    if (unlikely(errno_get(b)))
    {
        return errno_get(b);
    }

    *cur_block = b;

    return 0;
}

static int ext2_traverse_blocks(
    ext2_data_t* data,
    ext2_inode_t* raw_inode,
    size_t offset,
    size_t count,
    void* cb_data,
    block_cb_t cb)
{
    int errno;
    buffer_t* cur_block = NULL;
    level_t levels[4];
    level_t* cur_lvl;

    size_t left, to_copy;
    size_t block_nr     = offset / EXT2_BLOCK_SIZE;
    size_t block_offset = offset % EXT2_BLOCK_SIZE;

    if (unlikely(errno = ext2_traverse_blocks_init(data, raw_inode, block_nr, levels, &cur_lvl, &cur_block)))
    {
        return errno;
    }

    if (unlikely(!cur_block))
    {
        log_error("ext2_traverse_blocks_init returned NULL cur_block");
        return -EINVAL;
    }

    for (left = count = min(count, raw_inode->size - offset);
        left;
        left -= to_copy, block_offset = 0, errno = ext2_next_block(data, &cur_lvl, &cur_block))
    {
        if (unlikely(errno))
        {
            return errno;
        }

        to_copy = min(EXT2_BLOCK_SIZE - block_offset, left);

        if (cb(shift(cur_block->data, block_offset), to_copy, cb_data) == TRAVERSE_STOP)
        {
            break;
        }
    }

    return count;
}

static cmd_t ext2_read_block(void* block, size_t to_copy, void* data)
{
    char** buffer = data;
    memcpy(*buffer, block, to_copy);
    *buffer += to_copy;
    return TRAVERSE_CONTINUE;
}

static int ext2_read(file_t* file, char* buffer, size_t count)
{
    int res, errno;
    ext2_data_t* data = file->dentry->inode->sb->fs_data;
    ext2_inode_t* raw_inode = file->dentry->inode->fs_data;

    res = ext2_traverse_blocks(
        data,
        raw_inode,
        file->offset,
        count,
        &buffer,
        &ext2_read_block);

    if (unlikely(errno = errno_get(res)))
    {
        return errno;
    }

    file->offset += res;

    return res;
}

typedef struct readpage_context readpage_context_t;

struct readpage_context
{
    size_t offset; // Offset within current page
    page_t* current_page;
};

static cmd_t ext2_nopage_block(void* block, size_t to_copy, void* data)
{
    readpage_context_t* ctx = data;
    page_t* current_page = ctx->current_page;

    memcpy(shift(page_virt_ptr(current_page), ctx->offset), block, to_copy);

    if ((ctx->offset += to_copy) == PAGE_SIZE)
    {
        ctx->current_page = list_next_entry(&current_page->list_entry, page_t, list_entry);
        ctx->offset = 0;
    }

    return TRAVERSE_CONTINUE;
}

static int ext2_nopage(vm_area_t* vma, uintptr_t address, page_t** page)
{
    int res, errno;
    ext2_inode_t* inode = vma->dentry->inode->fs_data;
    ext2_data_t* data = vma->dentry->inode->sb->fs_data;

    *page = page_alloc1();

    if (unlikely(!*page))
    {
        return -ENOMEM;
    }

    readpage_context_t ctx = {
        .current_page = *page,
        .offset = 0,
    };

    off_t vma_off = address - vma->start;

    res = ext2_traverse_blocks(data, inode, vma->offset + vma_off, PAGE_SIZE, &ctx, &ext2_nopage_block);

    if (unlikely(errno = errno_get(res)))
    {
        pages_free(*page);
        return errno;
    }

    return 0;
}

static int ext2_mmap(file_t*, vm_area_t* vma)
{
    vma->ops = &ext2_vmops;
    return 0;
}

static inline char ext2_file_type_convert(uint16_t type)
{
    switch (type)
    {
        case 1: return DT_REG;
        case 2: return DT_DIR;
        case 7: return DT_LNK;
        default: return DT_UNKNOWN;
    }
}

typedef struct readdir_context readdir_context_t;

struct readdir_context
{
    void* buf;
    direntadd_t dirent_add;
    int i;
    ino_t parent_ino;
};

static cmd_t ext2_readdir_block(void* block, size_t to_copy, void* data)
{
    int over;
    readdir_context_t* ctx = data;
    ext2_dir_entry_t* dirent;
    size_t total_len;

    for (dirent = block, total_len = 0;
        total_len < to_copy;
        total_len += dirent->rec_len, dirent = ptr(addr(dirent) + dirent->rec_len), ++ctx->i)
    {
        log_debug(DEBUG_EXT2FS, "dirent: type=%x, inode=%u, type=%x, name=%S, rec_len=%u, len=%u",
            dirent->file_type,
            dirent->inode,
            dirent->file_type,
            dirent->name,
            dirent->rec_len,
            dirent->name_len);

        if (ctx->parent_ino == EXT2_LOST_FOUND_INO && ctx->i == 2)
        {
            return TRAVERSE_STOP;
        }

        over = ctx->dirent_add(ctx->buf, dirent->name, dirent->name_len, dirent->inode, ext2_file_type_convert(dirent->file_type));

        if (over)
        {
            return TRAVERSE_STOP;
        }
    }
    return TRAVERSE_CONTINUE;
}

static int ext2_readdir(file_t* file, void* buf, direntadd_t dirent_add)
{
    int res, errno;
    ext2_data_t* data = file->dentry->inode->sb->fs_data;
    ext2_inode_t* raw_inode = file->dentry->inode->fs_data;
    readdir_context_t ctx = {
        .buf = buf,
        .dirent_add = dirent_add,
        .i = 0,
        .parent_ino = file->dentry->inode->ino
    };

    if (unlikely(!S_ISDIR(raw_inode->mode)))
    {
        return -ENOTDIR;
    }

    if (!raw_inode->blocks)
    {
        return 0;
    }

    res = ext2_traverse_blocks(data, raw_inode, 0, raw_inode->size, &ctx, &ext2_readdir_block);

    if (unlikely(errno = errno_get(res)))
    {
        return res;
    }

    return ctx.i;
}

static int ext2_readlink(inode_t* inode, char* buffer, size_t size)
{
    ext2_inode_t* raw_inode = inode->fs_data;

    if (unlikely(!S_ISLNK(inode->mode)))
    {
        return -EINVAL;
    }

    if (size < inode->size)
    {
        return -ENAMETOOLONG;
    }

    if (inode->size <= 60)
    {
        memcpy(buffer, raw_inode->block, inode->size);
        return inode->size;
    }

    return -ENAMETOOLONG;
}

static int ext2_mount(super_block_t* sb, inode_t* inode, void*, int)
{
    int errno;
    buffer_t* b;
    ext2_sb_t* raw_sb;
    ext2_inode_t* root;
    ext2_data_t* data;

    if (!(data = alloc(ext2_data_t)))
    {
        return -ENOMEM;
    }

    b = block_read(sb->dev, sb->device_file, 1);

    if ((errno = errno_get(b)))
    {
        log_warning("cannot read block!");
        return errno;
    }

    raw_sb = ptr(b->data);

    if (raw_sb->magic != EXT2_SIGNATURE)
    {
        log_info("invalid signature: %x", raw_sb->magic);
        return -ENODEV;
    }

    sb->ops = &ext2_sb_ops;
    sb->module = this_module;
    sb->block_size = EXT2_SUPERBLOCK_OFFSET << raw_sb->log_block_size;

    if (sb->block_size != EXT2_BLOCK_SIZE)
    {
        log_info("unsupported block size: %u", sb->block_size);
        return -EINVAL;
    }

    data->addr_per_block = sb->block_size / sizeof(uint32_t);
    data->last_ind_block = EXT2_IND_BLOCK + data->addr_per_block - 1;
    data->first_dind_block = data->last_ind_block + 1;
    data->last_dind_block = data->first_dind_block + data->addr_per_block * data->addr_per_block - 1;
    data->sb = raw_sb;
    data->dev = sb->dev;
    data->file = sb->device_file;

    sb->fs_data = data;

    uint32_t inodes_count = raw_sb->inodes_count;
    uint32_t blocks_count = raw_sb->blocks_count;
    uint32_t blocks_per_group = raw_sb->blocks_per_group;
    uint32_t inodes_per_group = raw_sb->inodes_per_group;

    uint32_t block_group_count = align(blocks_count, blocks_per_group) / blocks_per_group;
    uint32_t block_group_count2 = align(inodes_count, inodes_per_group) / inodes_per_group;

    if (unlikely(block_group_count != block_group_count2))
    {
        log_warning("bad fs");
        delete(data);
        return -EINVAL;
    }

    data->inodes_per_group = inodes_per_group;
    data->inodes_per_block = EXT2_BLOCK_SIZE / sizeof(ext2_inode_t);

    root = ext2_inode_get(data, EXT2_ROOT_INO);

    if (unlikely(!root))
    {
        log_warning("cannot read root");
        delete(data);
        return -EINVAL;
    }

    inode->ino = EXT2_ROOT_INO;
    inode->ops = &ext2_inode_ops;
    inode->fs_data = root;
    inode->file_ops = &ext2_fops;
    inode->size = root->size;
    inode->sb = sb;
    inode->mode = root->mode;
    inode->ctime = root->ctime;
    inode->mtime = root->mtime;
    inode->nlink = root->links_count;

    return 0;
}

UNMAP_AFTER_INIT int ext2_init()
{
    return file_system_register(&ext2);
}

int ext2_deinit()
{
    return 0;
}
