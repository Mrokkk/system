#include "ext2.h"

#include <kernel/fs.h>
#include <kernel/page.h>
#include <kernel/ctype.h>
#include <kernel/kernel.h>
#include <kernel/minmax.h>
#include <kernel/module.h>
#include <kernel/process.h>

#include <arch/multiboot.h>

KERNEL_MODULE(ext2fs);
module_init(ext2_init);
module_exit(ext2_deinit);

int ext2_lookup(inode_t* parent, const char* name, inode_t** result);
int ext2_open(file_t* file);
int ext2_readdir(file_t* file, void* buf, direntadd_t dirent_add);
int ext2_mmap(file_t* file, void** data);
int ext2_mmap2(file_t* file, vm_area_t* vma, size_t offset);
int ext2_mount(super_block_t* sb, inode_t* inode, void*, int);
int ext2_read(file_t* file, char* buffer, size_t count);

static struct file_system ext2 = {
    .name = "ext2",
    .mount = &ext2_mount,
};

static struct super_operations ext2_sb_ops = {
};

static struct file_operations ext2_fops = {
    .open = &ext2_open,
    .read = &ext2_read,
    .readdir = &ext2_readdir,
    .mmap = &ext2_mmap,
    .mmap2 = &ext2_mmap2,
};

static struct inode_operations ext2_inode_ops = {
    .lookup = &ext2_lookup,
};

static inline uint32_t ext2_block_size(ext2_sb_t* sb)
{
    return EXT2_SUPERBLOCK_OFFSET << sb->log_block_size;
}

static void* block(ext2_sb_t* sb, size_t block)
{
    uint32_t block_size = ext2_block_size(sb);
    return ptr(addr(sb) + block_size * (block - 1));
}

static ext2_inode_t* ext2_inode_get(ext2_sb_t* sb, uint32_t ino)
{
    ext2_bgd_t* bgd = ptr(addr(sb) + sizeof(ext2_sb_t));
    ext2_inode_t* inode = block(sb, bgd->inode_table);
    return inode + (ino - 1);
}

int ext2_lookup_raw(ext2_sb_t* sb, ext2_inode_t* parent_inode, const char* name, ext2_inode_t** child_inode, uint16_t* ino)
{
    ext2_dir_entry_t* dirent;

    for (dirent = block(sb, parent_inode->block[0]);
        dirent->name_len;
        dirent = ptr(addr(dirent) + dirent->rec_len))
    {
        log_debug(DEBUG_EXT2FS, "dirent: type=%x, inode=%u, name=\"%s\"",
            dirent->file_type,
            dirent->inode,
            dirent->name);

        if (!strncmp(name, dirent->name, dirent->name_len))
        {
            *child_inode = ext2_inode_get(sb, dirent->inode);
            *ino = dirent->inode;

            log_debug(DEBUG_EXT2FS, "found inode; ino=%u, size=%u, name=%S",
                dirent->inode,
                (*child_inode)->size,
                dirent->name);

            return 0;
        }
    }

    return -ENOENT;
}

int ext2_lookup(struct inode* parent, const char* name, struct inode** result)
{
    int errno;
    uint16_t ino;
    ext2_inode_t* child_inode;
    ext2_inode_t* parent_inode = parent->fs_data;
    ext2_sb_t* sb = parent->sb->fs_data;

    if ((errno = ext2_lookup_raw(sb, parent_inode, name, &child_inode, &ino)))
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

int ext2_open(struct file*)
{
    return 0;
}

#define off(p, o) ptr(addr(p) + o)

int ext2_read(file_t* file, char* buffer, size_t count)
{
    char* data;
    size_t to_copy, left;
    uint32_t* ind_block_nr;
    ext2_sb_t* sb = file->inode->sb->fs_data;
    ext2_inode_t* raw_inode = file->inode->fs_data;

    uint32_t block_size = ext2_block_size(sb);
    size_t block_nr = file->offset / block_size;
    size_t block_offset = file->offset % block_size;

    left = count = min(count, raw_inode->size - file->offset);

    ind_block_nr = file->offset + count > EXT2_NDIR_BLOCKS * block_size
        ? block(sb, raw_inode->block[EXT2_IND_BLOCK])
        : NULL;

    while (left)
    {
        to_copy = min(block_size - block_offset, left);
        if (block_nr < EXT2_IND_BLOCK)
        {
            data = (char*)block(sb, raw_inode->block[block_nr]) + block_offset;
            log_debug(DEBUG_EXT2FS, "copying %u B from direct block %x to %x", to_copy, data, buffer);
            memcpy(buffer, (char*)block(sb, raw_inode->block[block_nr]) + block_offset, to_copy);
            buffer += to_copy;
            ++block_nr;
            block_offset = 0;
        }
        else if (block_nr == EXT2_IND_BLOCK && ind_block_nr)
        {
            data = (char*)block(sb, *ind_block_nr++) + block_offset;
            log_debug(DEBUG_EXT2FS, "copying %u B from indirect block %x to %x", to_copy, data, buffer);
            memcpy(buffer, data, to_copy);
            buffer += to_copy;
            block_offset = 0;
        }
        left -= to_copy;
    }

    file->offset += count;

    return count;
}

int ext2_mmap(file_t* file, void** data)
{
    ext2_sb_t* sb = file->inode->sb->fs_data;
    ext2_inode_t* raw_inode = file->inode->fs_data;
    *data = block(sb, raw_inode->block[0]);
    return 0;
}

int ext2_mmap2(file_t* file, vm_area_t* vma, size_t offset)
{
    ext2_inode_t* raw_inode = file->inode->fs_data;
    ext2_sb_t* sb = file->inode->sb->fs_data;
    uint32_t cur = offset, pos = offset, data_size;
    uint32_t block_size = ext2_block_size(sb);
    uint32_t block_nr = cur / block_size;
    page_t* current_page;
    page_t* pages;

    pages = page_alloc(vma->size / PAGE_SIZE, PAGE_ALLOC_DISCONT);

    if (unlikely(!pages))
    {
        log_debug(DEBUG_EXT2FS, "cannot allocate pages");
        return -ENOMEM;
    }

    current_page = pages;

    for (; block_nr < EXT2_NDIR_BLOCKS && pos < raw_inode->size && pos < vma->size; )
    {
        void* block_ptr = block(sb, raw_inode->block[block_nr]);
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

    uint32_t* ind_block = block(sb, raw_inode->block[EXT2_IND_BLOCK]);
    uint32_t* ind_block_end = off(block(sb, raw_inode->block[EXT2_IND_BLOCK]), block_size);

    for (; ind_block < ind_block_end && pos < raw_inode->size && pos < vma->size; )
    {
        void* block_ptr = block(sb, *ind_block);
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
    ext2_sb_t* sb = file->inode->sb->fs_data;
    ext2_inode_t* raw_inode = file->inode->fs_data;

    if (!S_ISDIR(raw_inode->mode))
    {
        return -ENOTDIR;
    }

    if (!raw_inode->blocks)
    {
        return 0;
    }

    end = block(sb, raw_inode->block[0] + 1);

    for (dirent = block(sb, raw_inode->block[0]);
        dirent < end;
        dirent = ptr(addr(dirent) + dirent->rec_len), ++i)
    {
        log_info("dirent: type=%x, inode=%u, type=%x, name=%S, rec_len=%u, len=%u",
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
    mbr_t* mbr;
    ext2_sb_t* raw_sb;
    ext2_inode_t* root;

    mbr = disk_img;
    raw_sb = ptr(addr(disk_img) + mbr->partitions[0].lba_start * MBR_SECTOR_SIZE + EXT2_SUPERBLOCK_OFFSET);

    sb->ops = &ext2_sb_ops;
    sb->module = this_module;
    sb->fs_data = raw_sb;

    uint32_t inodes_count = raw_sb->inodes_count;
    uint32_t blocks_count = raw_sb->blocks_count;
    uint32_t blocks_per_group = raw_sb->blocks_per_group;
    uint32_t inodes_per_group = raw_sb->inodes_per_group;

    uint32_t block_group_count = align_to_block_size(blocks_count, blocks_per_group) / blocks_per_group;
    uint32_t block_group_count2 = align_to_block_size(inodes_count, inodes_per_group) / inodes_per_group;

    if (unlikely(block_group_count != block_group_count2))
    {
        return -EINVAL; // TODO pick better errno
    }

    root = ext2_inode_get(raw_sb, EXT2_ROOT_INO);

    inode->ino = EXT2_ROOT_INO;
    inode->ops = &ext2_inode_ops;
    inode->fs_data = root;
    inode->file_ops = &ext2_fops;
    inode->size = root->size;
    inode->sb = sb;
    inode->fs_data = root;
    inode->mode = root->mode;

    return 0;
}

int ext2_init()
{
    file_system_register(&ext2);
    return 0;
}

int ext2_deinit()
{
    return 0;
}
