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
static int iso9660_mmap(file_t* file, vm_area_t* vma);
static int iso9660_read(file_t* file, char* buffer, size_t count);
static int iso9660_open(file_t* file);
static int iso9660_readdir(file_t* file, void* buf, direntadd_t dirent_add);
static int iso9660_mount(super_block_t* sb, inode_t* inode, void*, int);
static int iso9660_nopage(vm_area_t* vma, uintptr_t address, page_t** page);

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

static vm_operations_t iso9660_vmops = {
    .nopage = &iso9660_nopage,
};

static uint32_t block_nr_convert(uint32_t iso_block_nr)
{
    return (iso_block_nr * ISO9660_BLOCK_SIZE) / BLOCK_SIZE;
}

static ino_t ino_get(const buffer_t* block, void* ptr)
{
    return block->block * BLOCK_SIZE + (addr(ptr) - addr(block->data));
}

static buffer_t* block(iso9660_data_t* data, uint32_t block)
{
    log_debug(DEBUG_ISO9660, "reading block %x", block);
    return block_read(data->dev, data->file, block_nr_convert(block));
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

    if (addr(dirent) - addr(b->data) > BLOCK_SIZE)
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

typedef int (*iso9660_visitor_t)(iso9660_dirent_t* dirent, rrip_t* px, rrip_t* nm, buffer_t* b, void* visitor_data);

static int iso9660_traverse(
    iso9660_data_t* data,
    iso9660_dirent_t* parent_dirent,
    void* visitor_data,
    iso9660_visitor_t visitor,
    iso9660_dirent_t** result_dirent,
    rrip_t** result_px,
    rrip_t** result_nm,
    buffer_t** result_b)
{
    int errno, ret;
    iso9660_dirent_t* dirent;
    size_t dirents_len = GET(parent_dirent->data_len);
    buffer_t* b = block(data, GET(parent_dirent->lba));
    rrip_t* px;
    rrip_t* nm;

    if (unlikely((errno = errno_get(b))))
    {
        log_debug(DEBUG_ISO9660, "cannot read block %u", GET(parent_dirent->lba));
        return errno;
    }

    for (dirent = b->data; dirents_len; errno = iso9660_next_entry(data, &dirent, &b, &dirents_len))
    {
        if (errno)
        {
            log_debug(DEBUG_ISO9660, "cannot go to next entry: %d", errno);
            return errno;
        }

        if (dirent->name_len == 1 && (dirent->name[0] == 0 || dirent->name[0] == 1))
        {
            px = NULL;
            nm = NULL;
        }
        else if (iso9660_px_nm_find(dirent, &px, &nm))
        {
            log_debug(DEBUG_ISO9660, "cannot find required entries: PX=%x, NM=%x", px, nm);
            continue;
        }

        if ((ret = visitor(dirent, px, nm, b, visitor_data)))
        {
            *result_dirent = dirent;
            *result_px = px;
            *result_nm = nm;
            *result_b = b;
            return ret;
        }
    }

    return 0;
}

typedef struct find_data find_data_t;
struct find_data
{
    const char* name;
    const size_t name_len;
};

enum find_result
{
    DIRENT_NOT_FOUND = 0,
    DIRENT_FOUND = 1,
};

static int iso9660_find_visitor(
    iso9660_dirent_t* dirent,
    rrip_t*,
    rrip_t* nm,
    buffer_t*,
    void* visitor_data)
{
    find_data_t* data = visitor_data;

    if (dirent->name_len == 1) // Ignore "." and ".." as VFS handles them on it's own
    {
        return DIRENT_NOT_FOUND;
    }

    size_t nm_name_len = NM_NAME_LEN(nm);
    return data->name_len == nm_name_len && !strncmp(data->name, nm->nm.name, nm_name_len)
        ? DIRENT_FOUND
        : DIRENT_NOT_FOUND;
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
    rrip_t* px;
    rrip_t* nm;
    buffer_t* b;

    find_data_t find_data = {.name = name, .name_len = strlen(name)};
    errno = iso9660_traverse(data, parent_dirent, &find_data, &iso9660_find_visitor, &dirent, &px, &nm, &b);

    if (errno == DIRENT_FOUND)
    {
        *result_dirent = dirent;
        *result_px = px;
        *result_nm = nm;
        *ino = ino_get(b, dirent);
        return 0;
    }

    return errno ? errno : -ENOENT;
}

typedef struct readdir_data readdir_data_t;
struct readdir_data
{
    int count;
    void* buf;
    direntadd_t dirent_add;
};

enum readdir_result
{
    READDIR_CONTINUE = 0,
    READDIR_STOP = 1,
};

static int iso9660_readdir_visitor(
    iso9660_dirent_t* dirent,
    rrip_t* px,
    rrip_t* nm,
    buffer_t* b,
    void* visitor_data)
{
    size_t name_len;
    ino_t ino = ino_get(b, dirent);
    readdir_data_t* data = visitor_data;
    direntadd_t dirent_add = data->dirent_add;
    void* buf = data->buf;
    int over;

    if (dirent->name_len == 1 && dirent->name[0] == 0) // .
    {
        over = dirent_add(buf, ".", 1, 0, DT_DIR);
    }
    else if (dirent->name_len == 1 && dirent->name[0] == 1) // ..
    {
        over = dirent_add(buf, "..", 2, ino, DT_DIR);
    }
    else
    {
        if (unlikely(!nm))
        {
            return READDIR_CONTINUE;
        }

        name_len = NM_NAME_LEN(nm);
        over = dirent_add(buf, nm->nm.name, name_len, ino, S_ISDIR((mode_t)px->px.mode) ? DT_DIR : DT_REG);
    }

    ++data->count;
    return over ? READDIR_STOP : READDIR_CONTINUE;
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

    if (unlikely((errno = inode_alloc(result))))
    {
        log_debug(DEBUG_ISO9660, "cannot get free inode for \"%s\"", name);
        return errno;
    }

    (*result)->ino = ino;
    (*result)->ops = &iso9660_inode_ops;
    (*result)->sb = parent->sb;
    (*result)->size = GET(dirent->data_len);
    (*result)->file_ops = &iso9660_fops;
    (*result)->fs_data = dirent;
    if (px)
    {
        (*result)->mode = (mode_t)px->px.mode;
        (*result)->uid = (uid_t)px->px.uid;
        (*result)->gid = (gid_t)px->px.gid;
        // FIXME: set time properly
        (*result)->ctime = 0;
        (*result)->mtime = 0;
    }
    else
    {
        (*result)->mode = parent->mode;
        (*result)->uid = parent->uid;
        (*result)->gid = parent->gid;
    }

    return 0;
}

static int iso9660_mmap(file_t*, vm_area_t* vma)
{
    vma->ops= &iso9660_vmops;
    return 0;
}

// FIXME: there is some bug somewhere when using iso:
// [       5.534779] /bin/test[3]: page fault #0x4 at 0x4800 caused by 0x0
// Under the eip is 0 (so it's "add %al, (%eax)"), so page was not properly
// read
static int iso9660_nopage(vm_area_t* vma, uintptr_t address, page_t** page)
{
    iso9660_dirent_t* dirent = vma->inode->fs_data;
    iso9660_data_t* data = vma->inode->sb->fs_data;

    if (unlikely(!dirent || !data))
    {
        log_error("internal error: sb->fs_data=%x, inode->fs_data=%x", data, dirent);
        return -EINVAL;
    }

    int errno;
    void* data_ptr;
    off_t offset = vma->offset + address - vma->start;
    uint32_t pos = offset, data_size;
    uint32_t block_nr = offset / BLOCK_SIZE + block_nr_convert(GET(dirent->lba));
    size_t blocks_count = PAGE_SIZE / BLOCK_SIZE;
    buffer_t* b;

    if (unlikely(!(*page = page_alloc1())))
    {
        log_debug(DEBUG_ISO9660, "cannot allocate pages");
        return -ENOMEM;
    }

    data_ptr = page_virt_ptr(*page);

    for (size_t i = 0; i < blocks_count; ++i)
    {
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

    return 0;

error:
    pages_free(*page);
    *page = NULL;
    return errno;
}

static int iso9660_read(file_t* file, char* buffer, size_t count)
{
    iso9660_dirent_t* dirent = file->inode->fs_data;
    iso9660_data_t* data = file->inode->sb->fs_data;

    if (unlikely(!dirent || !data))
    {
        log_error("internal error: sb->fs_data=%x, inode->fs_data=%x", data, dirent);
        backtrace_dump(KERN_ERR);
        return -EINVAL;
    }

    int errno;
    size_t block_nr = file->offset / ISO9660_BLOCK_SIZE;
    size_t block_offset = file->offset % ISO9660_BLOCK_SIZE;
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
        to_copy = min(ISO9660_BLOCK_SIZE - block_offset, left);
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
    iso9660_data_t* data = file->inode->sb->fs_data;
    iso9660_dirent_t* parent_dirent = file->inode->fs_data;

    int errno;
    iso9660_dirent_t* dirent;
    rrip_t* px;
    rrip_t* nm;
    buffer_t* b;

    readdir_data_t readdir_data = {.dirent_add = dirent_add, .buf = buf, .count = 0};
    errno = iso9660_traverse(data, parent_dirent, &readdir_data, &iso9660_readdir_visitor, &dirent, &px, &nm, &b);

    if (errno < 0)
    {
        return errno;
    }

    return readdir_data.count;
}

static int iso9660_mount(super_block_t* sb, inode_t* inode, void*, int)
{
    int errno;
    iso9660_sb_t* raw_sb;
    iso9660_pvd_t* pvd;
    iso9660_data_t* data;
    buffer_t* b = block_read(sb->dev, sb->device_file, block_nr_convert(ISO9660_START_BLOCK));

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

    if (unlikely(raw_sb->type != ISO9660_VOLUME_PRIMARY))
    {
        log_debug(DEBUG_ISO9660, "not a Primary Volume Descriptor");
        return -ENODEV;
    }

    if (unlikely(GET(raw_sb->pvd.block_size) != ISO9660_BLOCK_SIZE))
    {
        log_debug(DEBUG_ISO9660, "unsupported block size: %u", GET(raw_sb->pvd.block_size));
        return -EINVAL;
    }

    if (unlikely(!(data = alloc(iso9660_data_t))))
    {
        log_debug(DEBUG_ISO9660, "cannot allocate memory for data");
        return -ENOMEM;
    }

    data->dev = sb->dev;
    data->file = sb->device_file;
    data->raw_sb = raw_sb;
    data->root = &raw_sb->pvd.root;

    sb->ops = &iso9660_sb_ops;
    sb->module = this_module;
    sb->fs_data = data;

    pvd = &raw_sb->pvd;
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
