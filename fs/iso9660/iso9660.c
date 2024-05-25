#include "iso9660.h"

#include <kernel/fs.h>
#include <kernel/page.h>
#include <kernel/malloc.h>
#include <kernel/module.h>

#define DEBUG_ISO9660   1

KERNEL_MODULE(iso9660);
module_init(iso9660_init);
module_exit(iso9660_deinit);

int iso9660_lookup(inode_t* parent, const char* name, inode_t** result);
int iso9660_read(file_t* file, char* buffer, size_t count);
int iso9660_write(file_t* file, const char* buffer, size_t count);
int iso9660_open(file_t* file);
int iso9660_mkdir(inode_t* parent, const char* name, int, int, inode_t** result);
int iso9660_create(inode_t* parent, const char* name, int, int, inode_t** result);
int iso9660_readdir(file_t* file, void* buf, direntadd_t dirent_add);
int iso9660_mount(super_block_t* sb, inode_t* inode, void*, int);

static_assert(offsetof(iso9660_pvd_t, root) == 156 - 8);
static_assert(offsetof(iso9660_dentry_t, name) == 33);
static_assert(offsetof(iso9660_sb_t, pvd) == 8);
static_assert(offsetof(rrip_t, px) == 4);
static_assert(sizeof(px_t) == 40);

static file_system_t iso9660 = {
    .name = "iso9660",
    .mount = &iso9660_mount,
};

static super_operations_t iso9660_sb_ops;

#if 0
static file_operations_t iso9660_fops = {
    .read = &iso9660_read,
    .write = &iso9660_write,
    .open = &iso9660_open,
    .readdir = &iso9660_readdir,
};

static inode_operations_t iso9660_inode_ops = {
    .lookup = &iso9660_lookup,
    .mkdir = &iso9660_mkdir,
    .create = &iso9660_create
};
#endif

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
    return block_read(data->dev, data->file, convert_block_nr(block, data->block_size));
}

int iso9660_lookup(inode_t* parent, const char* name, inode_t** result)
{
    (void)parent; (void)name; (void)result;
    return -ENOSYS;
}

int iso9660_read(file_t* file, char* buffer, size_t count)
{
    (void)file; (void)buffer; (void)count;
    return -ENOSYS;
}

int iso9660_write(file_t* file, const char* buffer, size_t count)
{
    (void)file; (void)buffer; (void)count;
    return -ENOSYS;
}

int iso9660_open(file_t*)
{
    return -ENOSYS;
}

int iso9660_readdir(file_t* file, void* buf, direntadd_t dirent_add)
{
    (void)file; (void)buf; (void)dirent_add;
    return -ENOSYS;
}

int iso9660_mount(super_block_t* sb, inode_t* inode, void*, int)
{
    (void)inode;
    int errno;
    iso9660_sb_t* raw_sb;
    iso9660_pvd_t* pvd;
    iso9660_data_t* data;
    uint32_t block_size;
    buffer_t* b = block_read(sb->dev, sb->device_file, 32);

    if ((errno = errno_get(b)))
    {
        log_warning("cannot read block!");
        return errno;
    }

    raw_sb = b->data;

    if (strncmp(raw_sb->identifier, ISO9660_SIGNATURE, ISO9660_SIGNATURE_LEN))
    {
        log_debug(DEBUG_ISO9660, "not a ISO9660 file system");
        return -ENODEV;
    }

    if (unlikely(!(data = alloc(iso9660_data_t))))
    {
        return -ENOMEM;
    }

    sb->ops = &iso9660_sb_ops;
    sb->module = this_module;

    data->dev = sb->dev;
    data->file = sb->device_file;
    data->raw_sb = raw_sb;

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

    log_debug(DEBUG_ISO9660, "Primary Volume Descriptor:");
    log_debug(DEBUG_ISO9660, "  System Identifier: \"%.32s\"", GET(pvd->sysident));
    log_debug(DEBUG_ISO9660, "  Volume Identifier: \"%.32s\"", GET(pvd->volident));
    log_debug(DEBUG_ISO9660, "  Volume Space Size: %u blocks", GET(pvd->space_size));
    log_debug(DEBUG_ISO9660, "  Volume Set Size: %u disks", GET(pvd->set_size));
    log_debug(DEBUG_ISO9660, "  Volume Sequence Number: %u disk", GET(pvd->sequence_nr));
    log_debug(DEBUG_ISO9660, "  Logical Block Size: %u B", GET(pvd->block_size));
    log_debug(DEBUG_ISO9660, "  Path Table Size: %u B", GET(pvd->path_table_size));
    log_debug(DEBUG_ISO9660, "  Path Table Location: %u block", GET(pvd->path_table_location));
    log_debug(DEBUG_ISO9660, "  Root dir: len: %u B, lba: %u block", GET(pvd->root.len), GET(pvd->root.lba));

    uint32_t block_nr = GET(pvd->root.lba);
    b = block(data, block_nr);

    if ((errno = errno_get(b)))
    {
        log_warning("cannot read block with path table!");
        return errno;
    }

    iso9660_dentry_t* dentry = b->data;

    char* name = alloc_array(char, 256);
    while (dentry->len)
    {
        strncpy(name, dentry->name, dentry->name_len);
        name[dentry->name_len] = 0;
        log_debug(DEBUG_ISO9660, "    len: %u B, name len: %u, name: \"%s\", %s, size: %u B",
            GET(dentry->len),
            GET(dentry->name_len),
            GET(name),
            GET(dentry->flags) == 2 ? "directory" : "file",
            GET(dentry->data_len));

        rrip_t* rrip = shift_as(rrip_t*, dentry, align(sizeof(*dentry) + dentry->name_len, 2));

        while (rrip->len && addr(rrip) < addr(dentry) + dentry->len)
        {
            log_debug(DEBUG_ISO9660, "      type: %.2s, len: %u", &rrip->sig, rrip->len);
            if (rrip->sig == PX_SIGNATURE)
            {
                log_continue(": mode: %.6o",    (uint32_t)rrip->px.mode);
                log_continue(", uid: %u",       (uint32_t)rrip->px.uid);
                log_continue(", gid: %u",       (uint32_t)rrip->px.gid);
                if (rrip->len > 36)
                {
                    log_continue(", ino: %x", (uint32_t)rrip->px.ino);
                }
                else
                {
                    log_continue(", ino: %u (generated)", iso9660_ino_get(b, rrip));
                }
            }
            else if (rrip->sig == NM_SIGNATURE)
            {
                strncpy(name, rrip->nm.name, NM_NAME_LEN(rrip));
                name[NM_NAME_LEN(rrip)] = 0;
                log_continue(": flags: %x, name: \"%.256s\"", rrip->nm.flags, name);
            }
            rrip = shift(rrip, rrip->len);
        }

        dentry = ptr(addr(dentry) + dentry->len);
    }

    delete_array(name, 256);

    return -ENODEV;
}

UNMAP_AFTER_INIT int iso9660_init()
{
    return file_system_register(&iso9660);
}

int iso9660_deinit()
{
    return 0;
}
