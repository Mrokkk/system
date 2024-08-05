#define log_fmt(fmt) "diskimg: " fmt
#include <kernel/fs.h>
#include <kernel/mbr.h>
#include <kernel/devfs.h>
#include <kernel/kernel.h>
#include <kernel/module.h>

#include <arch/multiboot.h>

module_init(diskimg_init);
module_exit(diskimg_deinit);
KERNEL_MODULE(diskimg);

static int diskimg_open(file_t* file);
static int diskimg_read(file_t* file, char* buf, size_t count);

static file_operations_t ops = {
    .open = &diskimg_open,
    .read = &diskimg_read,
};

int diskimg_init(void)
{
    char buf[8];
    partition_t* p;
    mbr_t* mbr = disk_img;

    if (!mbr)
    {
        return 0;
    }

    if (mbr->signature != MBR_SIGNATURE)
    {
        log_warning("wrong MBR");
        return -ENODEV;
    }

    devfs_blk_register("img", MAJOR_BLK_DISKIMG, BLK_MINOR_DRIVE(0), &ops);

    for (int j = 0; j < 4; ++j)
    {
        p = mbr->partitions + j;

        if (!p->lba_start)
        {
            continue;
        }

        devfs_blk_register(
            ssnprintf(buf, sizeof(buf), "img%u", j),
            MAJOR_BLK_DISKIMG,
            BLK_MINOR(0, j),
            &ops);
    }

    return 0;
}

int diskimg_deinit(void)
{
    return 0;
}

static int diskimg_open(file_t*)
{
    return 0;
}

static int diskimg_read(file_t* file, char* buf, size_t count)
{
    mbr_t* mbr = disk_img;
    int partition = BLK_PARTITION(MINOR(file->inode->dev));
    uint32_t offset = file->offset;

    if (partition != BLK_NO_PARTITION)
    {
        offset += mbr->partitions[partition].lba_start * MBR_SECTOR_SIZE;
    }

    memcpy(buf, (char*)mbr + offset, count);

    file->offset += count;

    return count;
}
