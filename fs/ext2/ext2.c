#define log_fmt(fmt) "ext2: " fmt
#include "ext2.h"

#include <kernel/fs.h>
#include <kernel/page.h>
#include <kernel/ctype.h>
#include <kernel/kernel.h>
#include <kernel/minmax.h>
#include <kernel/module.h>
#include <kernel/process.h>

#include <arch/multiboot.h>

KERNEL_MODULE(ext2);
module_init(ext2_init);
module_exit(ext2_deinit);

int ext2_lookup(inode_t* parent, const char* name, inode_t** result);
int ext2_open(file_t* file);
int ext2_readdir(file_t* file, void* buf, direntadd_t dirent_add);
int ext2_mmap(file_t* file, vm_area_t* vma, size_t offset);
int ext2_mount(super_block_t* sb, inode_t* inode, void*, int);
int ext2_read(file_t* file, char* buffer, size_t count);

static file_system_t ext2 = {
    .name = "ext2",
    .mount = &ext2_mount,
};

static super_operations_t ext2_sb_ops = {
};

static file_operations_t ext2_fops = {
    .open = &ext2_open,
    .read = &ext2_read,
    .readdir = &ext2_readdir,
    .mmap = &ext2_mmap,
};

static inode_operations_t ext2_inode_ops = {
    .lookup = &ext2_lookup,
};

static inline void* block(ext2_data_t* data, size_t block)
{
    buffer_t* b = block_read(data->dev, data->file, block);
    return b->data;
}

static ext2_inode_t* ext2_inode_get(ext2_data_t* data, uint32_t ino)
{
    uint32_t index = (ino - 1) % data->inodes_per_group;
    uint32_t block_nr = (index * sizeof(ext2_inode_t)) / data->block_size;
    uint32_t off = index % data->inodes_per_block;
    ext2_bgd_t* bgd = ptr(addr(data->sb) + sizeof(ext2_sb_t));
    ext2_inode_t* inode = block(data, bgd->inode_table + block_nr);
    return inode + off;
}

static int ext2_lookup_raw(
    ext2_data_t* data,
    ext2_inode_t* parent_inode,
    const char* name,
    ext2_inode_t** child_inode,
    uint16_t* ino)
{
    ext2_dir_entry_t* dirent;
    size_t name_len = strlen(name);

    log_debug(DEBUG_EXT2FS, "name=%s", name);

    for (dirent = block(data, parent_inode->block[0]);
        dirent->rec_len && dirent->name_len;
        dirent = ptr(addr(dirent) + dirent->rec_len))
    {
        if (name_len == dirent->name_len && !strncmp(dirent->name, name, dirent->name_len))
        {
            *child_inode = ext2_inode_get(data, dirent->inode);
            *ino = dirent->inode;

            log_debug(DEBUG_EXT2FS, "found inode; ino=%u, size=%u, name=%S",
                dirent->inode,
                (*child_inode)->size,
                dirent->name);

            return 0;
        }
    }

    log_debug(DEBUG_EXT2FS, "no inode with name = %s", name);

    return -ENOENT;
}

int ext2_lookup(inode_t* parent, const char* name, inode_t** result)
{
    int errno;
    uint16_t ino;
    ext2_inode_t* child_inode;
    ext2_inode_t* parent_inode = parent->fs_data;
    ext2_data_t* data = parent->sb->fs_data;

    if ((errno = ext2_lookup_raw(data, parent_inode, name, &child_inode, &ino)))
    {
        return errno;
    }

    if (unlikely((errno = inode_get(result))))
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

    return 0;
}

int ext2_open(file_t*)
{
    return 0;
}

#define off(ptr, off)               (typeof(ptr))(addr(ptr) + off)
#define in_range(v, first, last)    ((v) >= (first) && (v) <= (last))

int ext2_read(file_t* file, char* buffer, size_t count)
{
    char* block_data;
    size_t to_copy, left;
    uint32_t* ind_block_nr = NULL;
    ext2_data_t* data = file->inode->sb->fs_data;
    ext2_inode_t* raw_inode = file->inode->fs_data;

    uint32_t block_size = data->block_size;
    size_t block_nr = file->offset / block_size;
    size_t block_offset = file->offset & (block_size - 1);

    left = count = min(count, raw_inode->size - file->offset);

    while (left)
    {
        to_copy = min(block_size - block_offset, left);
        if (block_nr < EXT2_NDIR_BLOCKS)
        {
            block_data = block(data, raw_inode->block[block_nr]);
        }
        else if (in_range(block_nr, EXT2_IND_BLOCK, data->last_ind_block))
        {
            if (!ind_block_nr)
            {
                ind_block_nr = block(data, raw_inode->block[EXT2_IND_BLOCK]);
                ind_block_nr += block_nr - EXT2_IND_BLOCK;
            }
            block_data = block(data, *ind_block_nr++);
        }
        else if (in_range(block_nr, data->first_dind_block, data->last_dind_block))
        {
            size_t dind_block_nr = block_nr - data->first_dind_block;
            size_t lvl1_block_nr = dind_block_nr >> 8;
            size_t lvl2_block_nr = dind_block_nr & 0xff;
            uint32_t* temp = block(data, raw_inode->block[EXT2_DIND_BLOCK]);
            temp = block(data, temp[lvl1_block_nr]);
            block_data = block(data, temp[lvl2_block_nr]);
        }
        else
        {
            break;
        }

        block_data += block_offset;
        log_debug(DEBUG_EXT2FS, "copying %u B from block %u at %x to %x", to_copy, block_nr, block_data, buffer);
        memcpy(buffer, block_data, to_copy);
        buffer += to_copy;
        ++block_nr;
        block_offset = 0;
        left -= to_copy;
    }

    file->offset += count - left;

    return count;
}

int ext2_mmap(file_t* file, vm_area_t* vma, size_t offset)
{
    ext2_inode_t* raw_inode = file->inode->fs_data;
    ext2_data_t* data = file->inode->sb->fs_data;
    uint32_t cur = offset, pos = offset, data_size;
    uint32_t block_size = data->block_size;
    uint32_t block_nr = cur / block_size;
    page_t* current_page;
    page_t* pages;
    size_t size = vma->end - vma->start;

    pages = page_alloc(size / PAGE_SIZE, PAGE_ALLOC_DISCONT);

    if (unlikely(!pages))
    {
        log_debug(DEBUG_EXT2FS, "cannot allocate pages");
        return -ENOMEM;
    }

    current_page = pages;

    log_debug(DEBUG_EXT2FS, "off = %x, size = %x", offset, size);

    for (; block_nr < EXT2_NDIR_BLOCKS && pos < raw_inode->size && pos < size + offset; )
    {
        void* block_ptr = block(data, raw_inode->block[block_nr]);
        void* data_ptr = off(page_virt_ptr(current_page), pos % PAGE_SIZE);
        data_size = min(block_size, raw_inode->size - pos);

        log_debug(DEBUG_EXT2FS, "copying %u B from %x to %x", data_size, block_ptr, data_ptr);

        memcpy(data_ptr, block_ptr, data_size);

        ++block_nr;
        cur += data_size;
        pos += data_size;

        if (block_nr % 4 == 0)
        {
            current_page = list_next_entry(&current_page->list_entry, page_t, list_entry);
        }
    }

    block_nr = cur / block_size - EXT2_NDIR_BLOCKS;
    log_debug(DEBUG_EXT2FS, "block_nr = %u", block_nr);
    uint32_t* ind_block = block(data, raw_inode->block[EXT2_IND_BLOCK]);
    ind_block += block_nr;
    uint32_t* ind_block_end = off(block(data, raw_inode->block[EXT2_IND_BLOCK]), block_size);

    for (; ind_block < ind_block_end && pos < raw_inode->size && pos < size + offset; )
    {
        void* block_ptr = block(data, *ind_block);
        void* data_ptr = off(page_virt_ptr(current_page), pos % PAGE_SIZE);
        data_size = min(block_size, raw_inode->size - pos);

        log_debug(DEBUG_EXT2FS, "copying %u B from indirect block %u:%x to %x",
            data_size,
            *ind_block,
            block_ptr,
            data_ptr);

        memcpy(data_ptr, block_ptr, data_size);

        ++block_nr;
        cur += data_size;
        pos += data_size;

        if (block_nr % 4 == 0)
        {
            current_page = list_next_entry(&current_page->list_entry, page_t, list_entry);
        }

        ++ind_block;
    }

    log_debug(DEBUG_EXT2FS, "read %u B", pos);

    list_merge(&vma->pages->head, &pages->list_entry);

    return 0;
}

static inline char ext2_file_type_convert(uint16_t type)
{
    switch (type)
    {
        case 1: return DT_REG;
        case 2: return DT_DIR;
        default: return DT_UNKNOWN;
    }
}

int ext2_readdir(file_t* file, void* buf, direntadd_t dirent_add)
{
    int i = 0;
    int over;
    ext2_dir_entry_t* dirent;
    ext2_dir_entry_t* end;
    ext2_data_t* data = file->inode->sb->fs_data;
    ext2_inode_t* raw_inode = file->inode->fs_data;

    if (!S_ISDIR(raw_inode->mode))
    {
        return -ENOTDIR;
    }

    if (!raw_inode->blocks)
    {
        return 0;
    }

    end = block(data, raw_inode->block[0] + 1);

    for (dirent = block(data, raw_inode->block[0]);
        dirent < end;
        dirent = ptr(addr(dirent) + dirent->rec_len), ++i)
    {
        log_debug(DEBUG_EXT2FS, "dirent: type=%x, inode=%u, type=%x, name=%S, rec_len=%u, len=%u",
            dirent->file_type,
            dirent->inode,
            dirent->file_type,
            dirent->name,
            dirent->rec_len,
            dirent->name_len);

        over = dirent_add(buf, dirent->name, dirent->name_len, dirent->inode, ext2_file_type_convert(dirent->file_type));

        if (over)
        {
            break;
        }
    }

    return i;
}

int ext2_mount(super_block_t* sb, inode_t* inode, void*, int)
{
    buffer_t* b;
    ext2_sb_t* raw_sb;
    ext2_inode_t* root;
    ext2_data_t* data;

    if (!(data = alloc(ext2_data_t)))
    {
        return -ENOMEM;
    }

    b = block_read(sb->dev, sb->device_file, 1);
    raw_sb = ptr(b->data);

    if (raw_sb->magic != EXT2_SIGNATURE)
    {
        return -ENODEV;
    }

    sb->ops = &ext2_sb_ops;
    sb->module = this_module;
    sb->block_size = EXT2_SUPERBLOCK_OFFSET << raw_sb->log_block_size;

    data->addr_per_block = sb->block_size / sizeof(uint32_t);
    data->last_ind_block = EXT2_IND_BLOCK + data->addr_per_block - 1;
    data->first_dind_block = data->last_ind_block + 1;
    data->last_dind_block = data->first_dind_block + data->addr_per_block * data->addr_per_block - 1;
    data->block_size = sb->block_size;
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
        delete(data);
        log_warning("bad fs");
        return -EINVAL; // TODO pick better errno
    }

    data->inodes_per_group = inodes_per_group;
    data->inodes_per_block = data->block_size / sizeof(ext2_inode_t);

    root = ext2_inode_get(data, EXT2_ROOT_INO);

    inode->ino = EXT2_ROOT_INO;
    inode->ops = &ext2_inode_ops;
    inode->fs_data = root;
    inode->file_ops = &ext2_fops;
    inode->size = root->size;
    inode->sb = sb;
    inode->mode = root->mode;

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
