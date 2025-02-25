#define log_fmt(fmt) "blkdev: " fmt
#include <kernel/dev.h>
#include <kernel/gpt.h>
#include <kernel/mbr.h>
#include <kernel/unit.h>
#include <kernel/devfs.h>
#include <kernel/blkdev.h>
#include <kernel/kernel.h>
#include <kernel/page_alloc.h>

#define MAX_BLOCK_SIZE 4096
#define BLKDEV_SLOTS   24

enum
{
    BLKDEV_NONE,
    BLKDEV_MBR,
    BLKDEV_GPT,
};

struct partition
{
    char     name[36];
    uint32_t start;
    uint32_t end;
};

typedef struct partition partition_t;

struct blkdev
{
    uint16_t      major;
    uint16_t      minor;
    size_t        sectors;
    size_t        block_size;
    uint8_t       block_shift;
    uint8_t       part_type;
    blkdev_ops_t* ops;
    void*         data;
    size_t        partition_count;
    partition_t*  partitions;
    char          devfs_name[16];
    char          vendor_id[40];
    char          model_id[64];
};

typedef struct blkdev blkdev_t;

static int blkdev_open(file_t* file);
static int blkdev_read(file_t* file, char* buf, size_t count);

static blkdev_t* ide_blkdevs[BLKDEV_SLOTS];
static blkdev_t* sata_blkdevs[BLKDEV_SLOTS];
static blkdev_t* cdrom_blkdevs[BLKDEV_SLOTS];

READONLY static file_operations_t fops = {
    .open = &blkdev_open,
    .read = &blkdev_read,
};

static const char* blkdev_part_type_string(uint8_t type)
{
    switch (type)
    {
        case BLKDEV_NONE: return "empty";
        case BLKDEV_MBR:  return "MBR";
        case BLKDEV_GPT:  return "GPT";
        default:          return "unknown";
    }
}

static bool blkdev_block_size_valid(size_t block_size)
{
    return block_size <= MAX_BLOCK_SIZE && (block_size % 512 == 0);
}

static uint8_t blkdev_block_shift_calculate(size_t block_size)
{
    uint8_t shift = 8;

    do
    {
        ++shift;
        block_size >>= 1;
    }
    while (block_size > 256);

    return shift;
}

static int blkdev_slot_find(blkdev_t** blkdevs)
{
    for (int i = 0; i < BLKDEV_SLOTS; ++i)
    {
        if (!blkdevs[i])
        {
            return i;
        }
    }

    return -ENOMEM;
}

static void blkdev_partition_register(blkdev_t* blkdev, int drive, int part_id)
{
    int errno;
    char part_name[16];

    snprintf(part_name, sizeof(part_name), "%s%u", blkdev->devfs_name, part_id + 1);

    if (unlikely(errno = devfs_blk_register(
        part_name,
        blkdev->major,
        BLK_MINOR(part_id, drive),
        &fops)))
    {
        log_continue("; failed to register: %s", errno_name(errno));
    }
}

static int blkdev_partitions_allocate(blkdev_t* blkdev, size_t partition_count)
{
    blkdev->partitions = alloc_array(partition_t, partition_count);

    if (unlikely(!blkdev->partitions))
    {
        log_info("cannot allocate memory for partition table");
        blkdev->partition_count = 0;
        return -ENOMEM;
    }

    blkdev->partition_count = partition_count;

    return 0;
}

static void blkdev_mbr_handle(blkdev_t* blkdev, mbr_t* mbr)
{
    size_t partition_count = mbr_partition_count(mbr);

    if (partition_count == 0)
    {
        blkdev->partition_count = 0;
        blkdev->partitions = NULL;
        return;
    }

    if (unlikely(blkdev_partitions_allocate(blkdev, partition_count)))
    {
        return;
    }

    size_t part_id = 0;

    MBR_FOR_EACH_PARTITION(mbr, p)
    {
        strlcpy(blkdev->partitions[part_id].name, mbr_partition_name(p), sizeof(blkdev->partitions[part_id].name));
        blkdev->partitions[part_id].start = p->lba_start;
        blkdev->partitions[part_id].end   = p->lba_start + p->sectors;
        part_id++;
    }
}

static void blkdev_gpt_handle(blkdev_t* blkdev)
{
    int errno;
    page_t* gpt_page = page_alloc(1, PAGE_ALLOC_ZEROED);

    if (unlikely(!gpt_page))
    {
        log_info("gpt: cannot allocate temp page");
        return;
    }

    page_t* page = page_alloc(1, PAGE_ALLOC_ZEROED);

    if (unlikely(!page))
    {
        log_info("gpt: cannot allocate temp page for partition table");
        pages_free(gpt_page);
        return;
    }

    gpt_t* gpt = page_virt_ptr(gpt_page);

    if (unlikely(errno = blkdev->ops->read(blkdev->data, 1, gpt, 1, false)))
    {
        log_info("gpt: failed to read: %s", errno_name(errno));
        goto free;
    }

    void* start = page_virt_ptr(page);
    void* end = start + PAGE_SIZE;

    if (unlikely(errno = blkdev->ops->read(
        blkdev->data,
        (uint32_t)gpt->lba_part_entry,
        start,
        PAGE_SIZE >> blkdev->block_shift,
        false)))
    {
        log_info("gpt: failed to read partition table: %s", errno_name(errno));
        goto free;
    }

    size_t partition_count = gpt_partition_count(start, end, gpt->part_entry_size);

    if (unlikely(blkdev_partitions_allocate(blkdev, partition_count)))
    {
        return;
    }

    size_t part_id = 0;

    GPT_FOR_EACH_PARTITION(entry, start, end, gpt->part_entry_size)
    {
        for (size_t j = 0; j < 36; ++j)
        {
            blkdev->partitions[part_id].name[j] = entry->name[j];
        }

        blkdev->partitions[part_id].start = (uint32_t)entry->lba_start;
        blkdev->partitions[part_id].end   = (uint32_t)entry->lba_end;

        part_id++;
    }

free:
    pages_free(gpt_page);
    pages_free(page);
}

static int blkdev_partitions_read(blkdev_t* blkdev)
{
    int errno;
    page_t* page = page_alloc(1, PAGE_ALLOC_ZEROED);

    if (unlikely(!page))
    {
        log_info("mbr: failed to allocate page");
        return -ENOMEM;
    }

    mbr_t* mbr = page_virt_ptr(page);

    if (unlikely(errno = blkdev->ops->read(blkdev->data, 0, mbr, 1, false)))
    {
        log_info("mbr: failed to read: %s", errno_name(errno));
        return errno;
    }

    if (mbr->signature != MBR_SIGNATURE)
    {
        blkdev->part_type = BLKDEV_NONE;
    }
    else if (mbr_is_gpt(mbr))
    {
        blkdev->part_type = BLKDEV_GPT;
        blkdev_gpt_handle(blkdev);
    }
    else
    {
        blkdev->part_type = BLKDEV_MBR;
        blkdev_mbr_handle(blkdev, mbr);
    }

    pages_free(page);

    return 0;
}

static void blkdev_print(blkdev_t* blkdev)
{
    log_info("%s: %s %s", blkdev->devfs_name, blkdev->vendor_id, blkdev->model_id);

    if (blkdev->major != MAJOR_BLK_CDROM)
    {
        char* unit;
        uint64_t size = (uint64_t)blkdev->block_size * blkdev->sectors;
        human_size(size, unit);

        log_continue(", type: %s, size: %zu %s, blk size: %zu, sectors: %zu",
            blkdev_part_type_string(blkdev->part_type),
            (size_t)size,
            unit,
            blkdev->block_size,
            blkdev->sectors);
    }
}

int blkdev_register(blkdev_char_t* blk, void* data, blkdev_ops_t* ops)
{
    int errno;
    char* dev_name_fmt;
    blkdev_t** blkdevs = NULL;

    if (unlikely(!data || !ops || !ops->read))
    {
        return -EINVAL;
    }

    if (unlikely(blk->major != MAJOR_BLK_CDROM && !blkdev_block_size_valid(blk->block_size)))
    {
        return -EINVAL;
    }

    switch (blk->major)
    {
        case MAJOR_BLK_IDE:
            dev_name_fmt = "hd%c";
            blkdevs = ide_blkdevs;
            break;
        case MAJOR_BLK_SATA:
            dev_name_fmt = "sd%c";
            blkdevs = sata_blkdevs;
            break;
        case MAJOR_BLK_CDROM:
            dev_name_fmt = "sr%u";
            blkdevs = cdrom_blkdevs;
            break;
        default:
            return -EINVAL;
    }

    int drive = blkdev_slot_find(blkdevs);

    if (unlikely(drive < 0))
    {
        return drive;
    }

    blkdev_t* blkdev = alloc(blkdev_t);

    if (!blkdev)
    {
        return -ENOMEM;
    }

    blkdev->data        = data;
    blkdev->major       = blk->major;
    blkdev->sectors     = blk->sectors;
    blkdev->block_size  = blk->block_size;
    blkdev->block_shift = blk->block_size ? blkdev_block_shift_calculate(blk->block_size) : 0;
    blkdev->ops         = ops;

    if (blk->vendor)
    {
        snprintf(blkdev->vendor_id, sizeof(blkdev->vendor_id), blk->vendor);
    }
    if (blk->model)
    {
        snprintf(blkdev->model_id, sizeof(blkdev->model_id), blk->model);
    }

    snprintf(blkdev->devfs_name, sizeof(blkdev->devfs_name), dev_name_fmt, blk->major == MAJOR_BLK_CDROM ? drive : drive + 'a');

    if (blk->major != MAJOR_BLK_CDROM)
    {
        if (unlikely(errno = blkdev_partitions_read(blkdev)))
        {
            delete(blkdev);
            return errno;
        }
    }
    else
    {
        blkdev->part_type = BLKDEV_NONE;
    }

    blkdev_print(blkdev);

    if (unlikely(errno = devfs_blk_register(
        blkdev->devfs_name,
        blk->major,
        BLK_MINOR_DRIVE(drive),
        &fops)))
    {
        delete(blkdev);
        return errno;
    }

    for (size_t i = 0; i < blkdev->partition_count; ++i)
    {
        partition_t* p = blkdev->partitions + i;
        log_info("  part%u: [%#010x - %#010x] %s", i, p->start, p->end, p->name);
        blkdev_partition_register(blkdev, drive, i);
    }

    blkdevs[drive] = blkdev;

    return 0;
}

static int blkdev_free_impl(blkdev_t** blkdevs, int slot)
{
    slot -= 'a';

    if (unlikely(slot >= BLKDEV_SLOTS || slot < 0))
    {
        return -EINVAL;
    }

    delete(blkdevs[slot]);
    blkdevs[slot] = NULL;

    return 0;
}

int blkdev_free(int major, int id)
{
    switch (major)
    {
        case MAJOR_BLK_IDE:
            return blkdev_free_impl(ide_blkdevs, id);
        case MAJOR_BLK_SATA:
            return blkdev_free_impl(sata_blkdevs, id);
        case MAJOR_BLK_CDROM:
            return blkdev_free_impl(cdrom_blkdevs, id);
    }

    return -EINVAL;
}

static int blkdev_open(file_t*)
{
    return 0;
}

static int blkdev_medium_detect(blkdev_t* blkdev)
{
    int errno;

    if (!blkdev->ops->medium_detect)
    {
        return -ENOSYS;
    }

    if (unlikely(errno = blkdev->ops->medium_detect(
        blkdev->data,
        &blkdev->block_size,
        &blkdev->sectors)))
    {
        return errno;
    }

    if (unlikely(!blkdev_block_size_valid(blkdev->block_size)))
    {
        return -EINVAL;
    }

    char* unit;
    uint64_t size = (uint64_t)blkdev->block_size * blkdev->sectors;
    human_size(size, unit);

    log_info("%s: detected medium: size: %zu %s, blk size: %zu, sectors: %zu",
        blkdev->devfs_name,
        (size_t)size,
        unit,
        blkdev->block_size,
        blkdev->sectors);

    blkdev->block_shift = blkdev_block_shift_calculate(blkdev->block_size);

    return errno;
}

static int blkdev_read(file_t* file, char* buf, size_t count)
{
    int errno;
    int drive = BLK_DRIVE(MINOR(file->dentry->inode->rdev));
    int partition = BLK_PARTITION(MINOR(file->dentry->inode->rdev));
    blkdev_t* blkdev = NULL;

    switch (MAJOR(file->dentry->inode->rdev))
    {
        case MAJOR_BLK_IDE:   blkdev = ide_blkdevs[drive]; break;
        case MAJOR_BLK_SATA:  blkdev = sata_blkdevs[drive]; break;
        case MAJOR_BLK_CDROM: blkdev = cdrom_blkdevs[drive]; break;
        default:              return -EINVAL;
    }

    if (unlikely(!blkdev))
    {
        return -EINVAL;
    }

    if (blkdev->major == MAJOR_BLK_CDROM && !blkdev->block_size)
    {
        if (unlikely(errno = blkdev_medium_detect(blkdev)))
        {
            return errno;
        }
    }

    if (unlikely(!blkdev->block_size || count % blkdev->block_size))
    {
        return -EINVAL;
    }

    uint32_t offset = file->offset >> blkdev->block_shift;
    uint32_t first_sector, last_sector, max_count;

    if (partition != BLK_NO_PARTITION)
    {
        first_sector = blkdev->partitions[partition].start;
        last_sector  = blkdev->partitions[partition].end;
    }
    else
    {
        first_sector = 0;
        last_sector  = blkdev->sectors;
    }

    offset += first_sector;

    max_count = (last_sector - offset) << blkdev->block_shift;
    count = count > max_count
        ? max_count
        : count;

    if (!count)
    {
        return 0;
    }

    if (unlikely(errno = blkdev->ops->read(blkdev->data, offset, buf, count >> blkdev->block_shift, true)))
    {
        return errno;
    }

    file->offset += count;

    return count;
}
