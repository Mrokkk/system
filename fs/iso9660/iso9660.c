#define log_fmt(fmt) "iso9660: " fmt
#include "iso9660.h"

#include <kernel/fs.h>
#include <kernel/page.h>
#include <kernel/malloc.h>
#include <kernel/minmax.h>
#include <kernel/module.h>
#include <kernel/backtrace.h>

#define DEBUG_ISO9660   0

KERNEL_MODULE(iso9660);
module_init(iso9660_init);
module_exit(iso9660_deinit);

static int iso9660_lookup(inode_t* parent, const char* name, inode_t** result);
static int iso9660_mmap(file_t* file, vm_area_t* vma, size_t offset);
static int iso9660_read(file_t* file, char* buffer, size_t count);
static int iso9660_open(file_t* file);
static int iso9660_readdir(file_t* file, void* buf, direntadd_t dirent_add);
static int iso9660_mount(super_block_t* sb, inode_t* inode, void*, int);

static_assert(offsetof(iso9660_pvd_t, root) == 156 - 8);
static_assert(offsetof(iso9660_dirent_t, name) == 33);
static_assert(offsetof(iso9660_sb_t, pvd) == 8);
static_assert(offsetof(rrip_t, px) == 4);
static_assert(sizeof(px_t) == 40);

static file_system_t iso9660 = {
    .name = "iso9660",
    .mount = &iso9660_mount,
};

static super_operations_t iso9660_sb_ops;

static inode_operations_t iso9660_inode_ops = {
    .lookup = &iso9660_lookup,
};

static file_operations_t iso9660_fops = {
    .read = &iso9660_read,
    .open = &iso9660_open,
    .readdir = &iso9660_readdir,
    .mmap = &iso9660_mmap,
};

static const char* iso9660_volume_desc_string(uint8_t type)
{
    switch (type)
    {
        case ISO9660_VOLUME_BOOT_RECORD: return "Boot Record";
        case ISO9660_VOLUME_PRIMARY: return "Primary Volume Descriptor";
        case ISO9660_VOLUME_SUPPLEMENTARY: return "Supplementary Volume Descriptor";
        case ISO9660_VOLUME_PARTITION: return "Volume Partition Descriptor";
        case ISO9660_VOLUME_TERMINATOR: return "Volume Descriptor Set Terminator";
        default: return "Reserved";
    }
}

static uint32_t convert_block_nr(uint32_t iso_block_nr, uint32_t iso_block_size)
{
    return (iso_block_nr * iso_block_size) / BLOCK_SIZE;
}

static ino_t iso9660_ino_get(const buffer_t* block, void* ptr)
{
    return block->block * BLOCK_SIZE + (addr(ptr) - addr(block->data));
}

static buffer_t* block(iso9660_data_t* data, uint32_t block)
{
    log_debug(DEBUG_ISO9660, "reading block %x", block);
    return block_read(data->dev, data->file, convert_block_nr(block, data->block_size));
}

static int iso9660_px_nm_find(iso9660_dirent_t* dirent, rrip_t** px, rrip_t** nm)
{
    rrip_t* rrip = shift_as(rrip_t*, dirent, align(sizeof(*dirent) + dirent->name_len, 2));

    *px = *nm = NULL;

    while (addr(rrip) < addr(dirent) + dirent->len && rrip->len)
    {
        switch (rrip->sig)
        {
            case PX_SIGNATURE: *px = rrip; break;
            case NM_SIGNATURE: *nm = rrip; break;
        }
        rrip = shift(rrip, rrip->len);
    }

    return !(*px && *nm);
}

static int iso9660_next_entry(
    iso9660_data_t* data,
    iso9660_dirent_t** result_dirent,
    buffer_t** result_b,
    size_t* dirents_len)
{
    int errno;
    iso9660_dirent_t* dirent = *result_dirent;
    size_t dirent_len = dirent->len;
    size_t dirent_to_next_block = BLOCK_SIZE - addr(dirent) % BLOCK_SIZE;
    buffer_t* b = *result_b;

    dirent = shift(dirent, dirent_len);

    if (addr(dirent) > addr(b->data) + BLOCK_SIZE)
    {
        b = block_read(data->dev, data->file, b->block + 1);

        if (unlikely((errno = errno_get(b))))
        {
            return errno;
        }
        *dirents_len -= dirent_len;
    }
    else if (!dirent->len)
    {
        if ((int)(*dirents_len -= dirent_to_next_block) <= 0)
        {
            *dirents_len = 0;
            return 0;
        }

        b = block_read(data->dev, data->file, b->block + 1);

        if (unlikely((errno = errno_get(b))))
        {
            return errno;
        }
        dirent = b->data;
        if (!dirent->len)
        {
            *dirents_len = 0;
        }
    }
    else
    {
        if ((int)(*dirents_len -= dirent_len) <= 0)
        {
            *dirents_len = 0;
            return 0;
        }
    }

    *result_b = b;
    *result_dirent = dirent;

    return 0;
}

static int iso9660_dirent_find(
    iso9660_data_t* data,
    iso9660_dirent_t* parent_dirent,
    const char* name,
    iso9660_dirent_t** result_dirent,
    ino_t* ino,
    rrip_t** result_px,
    rrip_t** result_nm)
{
    int errno;
    iso9660_dirent_t* dirent;
    rrip_t* px = NULL;
    rrip_t* nm = NULL;
    bool found = false;
    size_t name_len = strlen(name);
    size_t dirents_len = GET(parent_dirent->data_len);

    buffer_t* b = block(data, GET(parent_dirent->lba));

    if (unlikely((errno = errno_get(b))))
    {
        log_debug(DEBUG_ISO9660, "cannot read block %u", GET(parent_dirent->lba));
        return errno;
    }

    dirent = b->data;

    for (dirent = b->data; dirents_len; errno = iso9660_next_entry(data, &dirent, &b, &dirents_len))
    {
        if (errno)
        {
            log_debug(DEBUG_ISO9660, "cannot go to next entry: %d", errno);
            return errno;
        }

        if (dirent->name_len == 1 && dirent->name[0] == 0) // .
        {
            if (!strcmp(".", name))
            {
                found = true;
                break;
            }
            continue;
        }
        else if (dirent->name_len == 1 && dirent->name[0] == 1) // ..
        {
            if (!strcmp("..", name))
            {
                found = true;
                break;
            }
            continue;
        }

        if (iso9660_px_nm_find(dirent, &px, &nm))
        {
            log_debug(DEBUG_ISO9660, "cannot find required entries: PX=%x, NM=%x", px, nm);
            continue;
        }

        size_t nm_name_len = NM_NAME_LEN(nm);
        if (name_len == nm_name_len && !strncmp(name, nm->nm.name, nm_name_len))
        {
            found = true;
            break;
        }
    }

    *result_dirent = dirent;
    *result_px = px;
    *result_nm = nm;
    *ino = iso9660_ino_get(b, dirent);

    return found ? 0 : -ENOENT;
}

static int iso9660_lookup(inode_t* parent, const char* name, inode_t** result)
{
    int errno;
    iso9660_data_t* data = parent->sb->fs_data;
    iso9660_dirent_t* parent_dirent = parent->fs_data;
    iso9660_dirent_t* dirent = NULL;
    rrip_t* px = NULL;
    rrip_t* nm = NULL;
    ino_t ino;

    if ((errno = iso9660_dirent_find(data, parent_dirent, name, &dirent, &ino, &px, &nm)))
    {
        log_debug(DEBUG_ISO9660, "cannot find dirent with name \"%s\"", name);
        return errno;
    }

    if (unlikely((errno = inode_get(result))))
    {
        return errno;
    }

    (*result)->ino = ino;
    (*result)->ops = &iso9660_inode_ops;
    (*result)->sb = parent->sb;
    (*result)->size = GET(dirent->data_len);
    (*result)->file_ops = &iso9660_fops;
    (*result)->fs_data = dirent;
    (*result)->mode = px ? (umode_t)px->px.mode : 0777 | S_IFDIR;
    // FIXME: set those properly
    (*result)->uid = 0;
    (*result)->gid = 0;
    (*result)->ctime = 0;
    (*result)->mtime = 0;

    return 0;
}

static int iso9660_mmap(file_t* file, vm_area_t* vma, size_t offset)
{
    iso9660_dirent_t* dirent = file->inode->fs_data;
    iso9660_data_t* data = file->inode->sb->fs_data;

    if (unlikely(!dirent || !data))
    {
        log_error("internal error: sb->fs_data=%x, inode->fs_data=%x", data, dirent);
        return -EINVAL;
    }

    int errno;
    void* data_ptr;
    uint32_t pos = offset, data_size;
    uint32_t block_nr = offset / BLOCK_SIZE + (GET(dirent->lba) * data->block_size) / BLOCK_SIZE;
    size_t size = vma->end - vma->start;
    page_t* current_page;
    page_t* pages;
    size_t blocks_count = size / BLOCK_SIZE;
    buffer_t* b;

    if (unlikely(!(current_page = pages = page_alloc(size / PAGE_SIZE, PAGE_ALLOC_DISCONT))))
    {
        log_debug(DEBUG_ISO9660, "cannot allocate pages");
        return -ENOMEM;
    }

    data_ptr = page_virt_ptr(current_page);

    for (size_t i = 0; i < blocks_count; ++i)
    {
        if (i && i % 4 == 0)
        {
            current_page = list_next_entry(&current_page->list_entry, page_t, list_entry);
            data_ptr = page_virt_ptr(current_page);
        }

        b = block_read(data->dev, data->file, block_nr);

        if (unlikely(errno = errno_get(b)))
        {
            goto error;
        }

        data_size = min(BLOCK_SIZE, GET(dirent->data_len) - pos);

        log_debug(DEBUG_ISO9660, "copying %u B from block %u %x to %x", data_size, block_nr, b->data, data_ptr);

        memcpy(data_ptr, b->data, data_size);

        ++block_nr;
        pos += data_size;
        data_ptr = shift(data_ptr, data_size);
    }

    list_merge(&vma->pages->head, &pages->list_entry);

    return 0;

error:
    pages_free(pages);
    return errno;
}

static int iso9660_read(file_t* file, char* buffer, size_t count)
{
    iso9660_dirent_t* dirent = file->inode->fs_data;
    iso9660_data_t* data = file->inode->sb->fs_data;

    if (unlikely(!dirent || !data))
    {
        log_error("internal error: sb->fs_data=%x, inode->fs_data=%x", data, dirent);
        backtrace_dump(log_error);
        return -EINVAL;
    }

    int errno;
    size_t block_size = data->block_size;
    size_t block_nr = file->offset / block_size;
    size_t block_offset = file->offset % block_size;
    size_t left = count = min(count, GET(dirent->data_len) - file->offset);
    size_t to_copy;
    buffer_t* b;
    char* block_data;
    size_t lba = GET(dirent->lba);

    log_debug(DEBUG_ISO9660, "count: %u, dirent: %x, size: %u B, lba: %u, block_nr: %u",
        count,
        dirent,
        GET(dirent->data_len),
        lba,
        block_nr);

    while (left)
    {
        to_copy = min(block_size - block_offset, left);
        b = block(data, block_nr + lba);

        if (unlikely(errno = errno_get(b)))
        {
            return errno;
        }

        block_data = b->data;
        block_data += block_offset;

        log_debug(DEBUG_ISO9660, "copying %u B from block %u %x to %x", to_copy, block_nr, block_data, buffer);
        memcpy(buffer, block_data, to_copy);
        buffer += to_copy;
        ++block_nr;
        block_offset = 0;
        left -= to_copy;
    }

    file->offset += count - left;

    log_debug(DEBUG_ISO9660, "returning %u", count);

    return count;
}

static int iso9660_open(file_t*)
{
    return 0;
}

static int iso9660_readdir(file_t* file, void* buf, direntadd_t dirent_add)
{
    int i = 0;
    int errno;
    iso9660_data_t* data = file->inode->sb->fs_data;
    iso9660_dirent_t* dirent;
    iso9660_dirent_t* parent_dirent = file->inode->fs_data;
    rrip_t* px = NULL;
    rrip_t* nm = NULL;
    bool over;
    ino_t ino;
    buffer_t* b = block(data, GET(parent_dirent->lba));

    if (unlikely((errno = errno_get(b))))
    {
        log_debug(DEBUG_ISO9660, "cannot read block %u", GET(parent_dirent->lba));
        return errno;
    }

    dirent = b->data;
    size_t dirents_len = GET(parent_dirent->data_len);

    for (dirent = b->data; dirents_len; errno = iso9660_next_entry(data, &dirent, &b, &dirents_len))
    {
        ino = iso9660_ino_get(b, dirent);

        if (dirent->name_len == 1 && dirent->name[0] == 0) // .
        {
            over = dirent_add(buf, ".", 1, file->inode->ino, DT_DIR);
            ++i;
        }
        else if (dirent->name_len == 1 && dirent->name[0] == 1) // ..
        {
            over = dirent_add(buf, "..", 2, ino, DT_DIR);
            ++i;
        }
        else
        {
            if (iso9660_px_nm_find(dirent, &px, &nm))
            {
                log_debug(DEBUG_ISO9660, "cannot find required entries: PX=%x, NM=%x", px, nm);
                continue;
            }

            over = dirent_add(buf, nm->nm.name, NM_NAME_LEN(nm), ino, S_ISDIR(px->px.mode) ? DT_DIR : DT_REG);
            ++i;
        }

        if (over)
        {
            break;
        }
    }

    return i;
}

static int iso9660_mount(super_block_t* sb, inode_t* inode, void*, int)
{
    int errno;
    iso9660_sb_t* raw_sb;
    iso9660_pvd_t* pvd;
    iso9660_data_t* data;
    uint32_t block_size;
    buffer_t* b = block_read(sb->dev, sb->device_file, 32);

    if (unlikely(errno = errno_get(b)))
    {
        log_warning("cannot read block!");
        return errno;
    }

    raw_sb = b->data;

    if (strncmp(raw_sb->identifier, ISO9660_SIGNATURE, ISO9660_SIGNATURE_LEN))
    {
        log_debug(DEBUG_ISO9660, "not an ISO9660 file system");
        return -ENODEV;
    }

    if (unlikely(!(data = alloc(iso9660_data_t))))
    {
        log_debug(DEBUG_ISO9660, "cannot allocate memory for data");
        return -ENOMEM;
    }
    data->dev = sb->dev;
    data->file = sb->device_file;
    data->raw_sb = raw_sb;

    sb->ops = &iso9660_sb_ops;
    sb->module = this_module;
    sb->fs_data = data;

    log_debug(DEBUG_ISO9660, "type: %s (%x), identifier: %.5s, version: %x",
        iso9660_volume_desc_string(raw_sb->type),
        raw_sb->type,
        raw_sb->identifier,
        raw_sb->version);

    if (unlikely(raw_sb->type != ISO9660_VOLUME_PRIMARY))
    {
        return -ENODEV;
    }

    pvd = &raw_sb->pvd;
    block_size = GET(pvd->block_size);
    data->block_size = block_size;
    data->root = &pvd->root;

    inode->ino = ISO9660_ROOT_INO;
    inode->ops = &iso9660_inode_ops;
    inode->fs_data = &pvd->root;
    inode->file_ops = &iso9660_fops;
    inode->size = GET(pvd->root.data_len);
    inode->sb = sb;
    inode->mode = 0777 | S_IFDIR;

    log_debug(DEBUG_ISO9660, "Primary Volume Descriptor:");
    log_debug(DEBUG_ISO9660, "  System Identifier: \"%.32s\"", GET(pvd->sysident));
    log_debug(DEBUG_ISO9660, "  Volume Identifier: \"%.32s\"", GET(pvd->volident));
    log_debug(DEBUG_ISO9660, "  Volume Space Size: %u blocks", GET(pvd->space_size));
    log_debug(DEBUG_ISO9660, "  Volume Set Size: %u disks", GET(pvd->set_size));
    log_debug(DEBUG_ISO9660, "  Volume Sequence Number: %u disk", GET(pvd->sequence_nr));
    log_debug(DEBUG_ISO9660, "  Logical Block Size: %u B", GET(pvd->block_size));
    log_debug(DEBUG_ISO9660, "  Root dir: lba: %u block", GET(pvd->root.lba));

    return 0;
}

UNMAP_AFTER_INIT int iso9660_init()
{
    return file_system_register(&iso9660);
}

int iso9660_deinit()
{
    return 0;
}
