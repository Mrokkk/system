#define log_fmt(fmt) "ide: " fmt
#include "ide.h"

#include <arch/io.h>
#include <arch/dma.h>
#include <arch/pci.h>
#include <kernel/fs.h>
#include <kernel/irq.h>
#include <kernel/mbr.h>
#include <kernel/wait.h>
#include <kernel/page.h>
#include <kernel/devfs.h>
#include <kernel/mutex.h>
#include <kernel/device.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/process.h>

// https://pdos.csail.mit.edu/6.828/2018/readings/hardware/IDE-BusMaster.pdf
// https://wiki.osdev.org/PCI_IDE_Controller

#define CH(x)           (channels[x])
#define ERROR_REG(c)    CH(c).error_reg
#define NSECTOR_REG(c)  CH(c).nsector_reg
#define LBA0_REG(c)     CH(c).sector_reg
#define LBA1_REG(c)     CH(c).lcyl_reg
#define LBA2_REG(c)     CH(c).hcyl_reg
#define SELECT_REG(c)   CH(c).select_reg
#define CMD_REG(c)      CH(c).status_reg

static int ide_fs_open(file_t* file);
static int ide_fs_read(file_t* file, char* buf, size_t count);
static void ide_irq();
static void ide_write(uint8_t channel, uint8_t reg, uint8_t data);
static uint8_t ide_ata_access(uint8_t direction, uint8_t drive, uint32_t lba, uint8_t numsects, void* buf, int dma);

static ide_channel_t channels[2];
static ide_device_t ide_devices[4];
static uint8_t ide_buf[ATA_SECTOR_SIZE];
static dma_t* dma_region;

module_init(ide_initialize);
module_exit(ide_deinit);
KERNEL_MODULE(ide);

static WAIT_QUEUE_HEAD_DECLARE(ide_queue);

static file_operations_t ops = {
    .open = &ide_fs_open,
    .read = &ide_fs_read,
};

static uint8_t ide_read(uint8_t channel, uint8_t reg)
{
    return inb(channels[channel].base + reg);
}

static void ide_write(uint8_t channel, uint8_t reg, uint8_t data)
{
    outb(data, channels[channel].base + reg);
}

static void ide_read_buffer(uint8_t channel, uint8_t reg, void* buffer, uint32_t count)
{
    insl(channels[channel].base + reg, buffer, count);
}

static void ide_enable_irq(uint8_t channel)
{
    outb(0x00, channels[channel].ctrl + ATA_REG_CONTROL);
    io_delay(10);
}

static void ide_disable_irq(uint8_t channel)
{
    outb(0x02, channels[channel].ctrl + ATA_REG_CONTROL);
    io_delay(10);
}

static inline void ide_channel_fill(int channel, pci_device_t* ide_pci)
{
    uint16_t base, port;
    port = base = channel ? 0x170 : 0x1f0;
    channels[channel].data_reg = channels[channel].base = port++;
    channels[channel].error_reg = port++;
    channels[channel].nsector_reg = port++;
    channels[channel].sector_reg = port++;
    channels[channel].lcyl_reg = port++;
    channels[channel].hcyl_reg = port++;
    channels[channel].select_reg = port++;
    channels[channel].select_reg = port++;
    channels[channel].status_reg = port++;
    channels[channel].ctrl = base + 0x206;
    channels[channel].bmide = ide_pci->bar[4].addr & 0xfffffffc;
}

static inline const char* ide_error_string(int e)
{
    switch (e)
    {
        case 0: return "No error";
        case ATA_ER_BBK: return "Bad Block detected";
        case ATA_ER_UNC: return "Uncorrectable data error";
        case ATA_ER_MC: return "Media changed";
        case ATA_ER_IDNF: return "ID not found";
        case ATA_ER_MCR: return "Media change request";
        case ATA_ER_ABRT: return "Aborted command";
        case ATA_ER_TK0NF: return "Track zero not found";
        case ATA_ER_AMNF: return "Address mark not found";
        default: return "Unknown error";
    }
}

void ide_device_print(ide_device_t* device)
{
    const char* unit;
    uint64_t size = (uint64_t)device->size * ATA_SECTOR_SIZE;

    human_size(size, unit);

    log_info("%u: %s drive %u %s: %s; signature: %x",
        device->channel | device->drive << 1,
        device->type ? "ATAPI" : "ATA",
        (uint32_t)size,
        unit,
        device->model,
        device->signature);

    log_continue("; cap: %x", device->capabilities);
    if (device->dma)
    {
        log_continue("; DMA");
    }
    if (device->capabilities & 0x200)
    {
        log_continue("; LBA");
    }
    else
    {
        log_continue("; CHS");
    }
}

static void ide_device_register(int i)
{
    char buf[12];
    partition_t* p;
    mbr_t* mbr = ptr(ide_buf);

    ide_device_print(&ide_devices[i]);

    devfs_blk_register(fmtstr(buf, "hd%c", i + 'a'), MAJOR_BLK_IDE, BLK_MINOR_DRIVE(i), &ops);

    if (ide_devices[i].type == 1)
    {
        return;
    }

    if (ide_ata_access(ATA_READ, 0, 0, 1, ide_buf, 0))
    {
        log_warning("error reading MBR");
        return;
    }

    log_info("\tmbr: id = %x, signature = %x", mbr->id, mbr->signature);

    if (mbr->signature != MBR_SIGNATURE)
    {
        return;
    }

    ide_devices[i].mbr = mbr; // FIXME: allocate a new buffer for each possible drive

    for (int j = 0; j < 4; ++j)
    {
        p = mbr->partitions + j;

        if (!p->lba_start)
        {
            continue;
        }

        log_info("\tmbr: part[%u]:%x: [%08x - %08x]",
            j, p->type, p->lba_start, p->lba_start + p->sectors);

        devfs_blk_register(
            fmtstr(buf, "hd%c%u", i + 'a', j),
            MAJOR_BLK_IDE,
            BLK_MINOR(i, j),
            &ops);
    }
}

int ide_initialize(void)
{
    int i, j, k, count = 0;
    bool use_dma = true;
    pci_device_t* ide_pci = pci_device_get(0x01, 0x01);

    if (!ide_pci)
    {
        log_notice("no IDE controller");
        return -ENODEV;
    }

    pci_device_print(ide_pci);

    if (ide_pci->prog_if & 0x01)
    {
        log_warning("PCI native mode not supported yet");
        return -ENODEV;
    }

    if (!(ide_pci->prog_if & 0x80))
    {
        log_warning("bus mastering is not supported!");
        use_dma = false;
    }

    dma_region = region_map(DMA_PRD, 5 * PAGE_SIZE, "dma");

    if (unlikely(!dma_region))
    {
        log_warning("cannot allocate DMA region");
        use_dma = false;
    }
    else
    {
        memset(dma_region, 0, 5 * PAGE_SIZE);
    }

    ide_channel_fill(ATA_PRIMARY, ide_pci);
    ide_channel_fill(ATA_SECONDARY, ide_pci);

    ide_disable_irq(ATA_PRIMARY);
    ide_disable_irq(ATA_SECONDARY);

    // 3- Detect ATA-ATAPI Devices:
    for (i = 0; i < 2; i++)
    {
        for (j = 0; j < 2; j++)
        {
            uint8_t err = 0, type = IDE_ATA, status;

            // (I) Select Drive:
            ide_write(i, ATA_REG_HDDEVSEL, 0xa0 | (j << 4)); // Select Drive.
            io_delay(100);

            // (II) Send ATA Identify Command:
            ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            io_delay(100);

            // (III) Polling:
            if (ide_read(i, ATA_REG_STATUS) == 0)
            {
                continue; // If Status = 0, No Device.
            }

            while (1)
            {
                status = ide_read(i, ATA_REG_STATUS);
                if ((status & ATA_SR_ERR))
                {
                    err = 1;
                    break;
                }
                if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
                {
                    break;
                }
            }

            if (err)
            {
                uint8_t cl = ide_read(i, ATA_REG_LBA1);
                uint8_t ch = ide_read(i, ATA_REG_LBA2);

                if ((cl == 0x14 && ch == 0xeb) || (cl == 0x69 && ch == 0x96))
                {
                    type = IDE_ATAPI;
                }
                else
                {
                    continue;
                }

                ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
                io_delay(100);
            }

            // (V) Read Identification Space of the Device:
            ide_read_buffer(i, ATA_REG_DATA, ide_buf, 128);

            uint8_t bm_status = inb(channels[i].bmide + 2);

            // FIXME: this is a hack for QEMU, as apparently QEMU does not report this bit.
            // DMA is working fine on real HW (IBM ThinkPad T42), however on QEMU IRQ is fired
            // when transfer is still in progress (no data copied) and no further IRQ is received
            if (!(bm_status & BM_STATUS_DRV0_DMA))
            {
                ide_devices[count].dma = false;
            }
            else
            {
                ide_devices[count].dma = use_dma;
                outb(bm_status | BM_STATUS_INTERRUPT, channels[i].bmide);
                ide_enable_irq(i);
            }

            // (VI) Read Device Parameters:
            ide_devices[count].type         = type;
            ide_devices[count].channel      = i;
            ide_devices[count].drive        = j;
            ide_devices[count].signature    = *((uint16_t*)(ide_buf + ATA_IDENT_DEVICETYPE));
            ide_devices[count].capabilities = *((uint16_t*)(ide_buf + ATA_IDENT_CAPABILITIES));
            ide_devices[count].command_sets = *((uint32_t*)(ide_buf + ATA_IDENT_COMMANDSETS));

            // (VII) Get Size:
            if (ide_devices[count].command_sets & (1 << 26))
            {
                // Device uses 48-Bit Addressing:
                ide_devices[count].size   = *((uint32_t*)(ide_buf + ATA_IDENT_MAX_LBA_EXT));
            }
            else
            {
                // Device uses CHS or 28-bit Addressing:
                ide_devices[count].size   = *((uint32_t*)(ide_buf + ATA_IDENT_MAX_LBA));
            }

            // (VIII) String indicates model of device (like Western Digital HDD and SONY DVD-RW...):
            for (k = 0; k < 40; k += 2)
            {
                if (ide_buf[ATA_IDENT_MODEL + k + 1] < 0x20 || ide_buf[ATA_IDENT_MODEL + k + 1] > 0x7e)
                {
                    strcpy((char*)ide_devices[count].model, "<garbage>");
                    goto next;
                }
                ide_devices[count].model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
                ide_devices[count].model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];
            }

            for (; k; --k)
            {
                if (ide_devices[count].model[k] != ' ' && ide_devices[count].model[k] != 0)
                {
                    break;
                }
            }
            ide_devices[count].model[k + 1] = 0;

next:
            count++;
        }
    }

    for (i = 0; i < 4; i++)
    {
        if (!ide_devices[i].signature)
        {
            continue;
        }

        ide_device_register(i);
    }

    irq_register(14, &ide_irq, "ata1", IRQ_DEFAULT);

    return 0;
}

int ide_deinit(void)
{
    return 0;
}

static uint8_t ide_polling(uint8_t channel, uint32_t advanced_check)
{
    uint8_t status;

    // (I) Delay 400 nanosecond for BSY to be set:
    // -------------------------------------------------
    io_delay(10);

    // (II) Wait for BSY to be cleared:
    // -------------------------------------------------
    while (1)
    {
        status = ide_read(channel, ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
        {
            break;
        }
        io_delay(100);
    }

    if (advanced_check)
    {
        // (III) Check For Errors:
        // -------------------------------------------------
        if (status & ATA_SR_ERR)
        {
            return 2; // Error.
        }

        // (IV) Check If Device fault:
        // -------------------------------------------------
        if (status & ATA_SR_DF)
        {
            return 1; // Device Fault.
        }

        // (V) Check DRQ:
        // -------------------------------------------------
        // BSY = 0; DF = 0; ERR = 0 so we should check for DRQ now.
        if ((status & ATA_SR_DRQ) == 0)
        {
            return 3; // DRQ should be set
        }
   }

   return 0; // No Error.
}

enum mode
{
    IDE_CHS = 0,
    IDE_LBA28 = 1,
    IDE_LBA48 = 2,
};

static int ide_ata_prepare_transfer(uint8_t direction, uint8_t drive, uint32_t lba, uint8_t numsects, int dma)
{
    int lba_mode;
    uint8_t cmd;
    uint8_t lba_io[6];
    uint32_t channel = ide_devices[drive].channel;
    uint32_t slavebit = ide_devices[drive].drive;
    uint16_t cyl;
    uint8_t head, sect;

    // (I) Select one from LBA28, LBA48 or CHS;
    if (lba >= 0x10000000) // LBA48
    {
        lba_mode  = IDE_LBA48;
        lba_io[0] = (lba & 0x000000ff) >> 0;
        lba_io[1] = (lba & 0x0000ff00) >> 8;
        lba_io[2] = (lba & 0x00ff0000) >> 16;
        lba_io[3] = (lba & 0xff000000) >> 24;
        lba_io[4] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
        lba_io[5] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
        head      = 0; // Lower 4-bits of HDDEVSEL are not used here.
    }
    else if (ide_devices[drive].capabilities & 0x200) // LBA28
    {
        lba_mode  = IDE_LBA28;
        lba_io[0] = (lba & 0x00000ff) >> 0;
        lba_io[1] = (lba & 0x000ff00) >> 8;
        lba_io[2] = (lba & 0x0ff0000) >> 16;
        lba_io[3] = 0; // These Registers are not used here.
        lba_io[4] = 0; // These Registers are not used here.
        lba_io[5] = 0; // These Registers are not used here.
        head      = (lba & 0xf000000) >> 24;
    }
    else
    {
        // CHS:
        lba_mode  = IDE_CHS;
        sect      = (lba % 63) + 1;
        cyl       = (lba + 1  - sect) / (16 * 63);
        lba_io[0] = sect;
        lba_io[1] = (cyl >> 0) & 0xff;
        lba_io[2] = (cyl >> 8) & 0xff;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head      = (lba + 1  - sect) % (16 * 63) / (63); // Head number is written to HDDEVSEL lower 4-bits.
    }

    // (III) Wait if the drive is busy;
    while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY);

    // (IV) Select Drive from the controller;
    if (lba_mode == IDE_CHS)
    {
        ide_write(channel, ATA_REG_HDDEVSEL, 0xa0 | (slavebit << 4) | head); // Drive & CHS.
    }
    else
    {
        ide_write(channel, ATA_REG_HDDEVSEL, 0xe0 | (slavebit << 4) | head); // Drive & LBA
    }

    // (V) Write Parameters;
    if (lba_mode == IDE_LBA48)
    {
        ide_write(channel, ATA_REG_SECCOUNT1,   0);
        ide_write(channel, ATA_REG_LBA3,        lba_io[3]);
        ide_write(channel, ATA_REG_LBA4,        lba_io[4]);
        ide_write(channel, ATA_REG_LBA5,        lba_io[5]);
    }

    ide_write(channel, ATA_REG_SECCOUNT0,   numsects);
    ide_write(channel, ATA_REG_LBA0,        lba_io[0]);
    ide_write(channel, ATA_REG_LBA1,        lba_io[1]);
    ide_write(channel, ATA_REG_LBA2,        lba_io[2]);

    while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY);

    if (lba_mode == IDE_CHS     && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
    if (lba_mode == IDE_LBA28   && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
    if (lba_mode == IDE_LBA48   && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO_EXT;
    if (lba_mode == IDE_CHS     && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == IDE_LBA28   && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == IDE_LBA48   && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA_EXT;
    if (lba_mode == IDE_CHS     && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == IDE_LBA28   && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == IDE_LBA48   && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO_EXT;
    if (lba_mode == IDE_CHS     && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == IDE_LBA28   && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == IDE_LBA48   && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA_EXT;

    ide_write(channel, ATA_REG_COMMAND, cmd);

    return lba_mode;
}

static uint8_t ide_ata_access(uint8_t direction, uint8_t drive, uint32_t lba, uint8_t numsects, void* buf, int dma)
{
    int lba_mode;
    uint32_t channel = ide_devices[drive].channel;
    uint32_t words = ATA_SECTOR_SIZE / 2;
    uint16_t i;
    uint8_t err;

    lba_mode = ide_ata_prepare_transfer(direction, drive, lba, numsects, dma);

    if (dma)
    {
        outb(0, channels[channel].bmide);
        io_delay(2);

        dma_region->prdt[0].addr = DMA_START;
        dma_region->prdt[0].count = numsects * ATA_SECTOR_SIZE;
        dma_region->prdt[0].last = 0xffff;
        dma_region->prdt[1].addr = 0;
        dma_region->prdt[1].count = 0;
        dma_region->prdt[1].last = 0;

        outl(DMA_PRD, channels[channel].bmide + 4);
        io_delay(2);
        outb(BM_CMD_READ, channels[channel].bmide);
        io_delay(2);
        outb(inb(channels[channel].bmide + 2) | BM_STATUS_ERROR | BM_STATUS_INTERRUPT, channels[channel].bmide + 2);
        io_delay(2);

        log_info("dma: ch%u PRD: addr: %x, size: %x, last: %x",
            channel,
            dma_region->prdt[0].addr,
            dma_region->prdt[0].count,
            dma_region->prdt[0].last);

        outb(BM_CMD_START | BM_CMD_READ, channels[channel].bmide);
        return 0;
    }

    if (direction == 0)
    {
        // PIO Read.
        for (i = 0; i < numsects; i++)
        {
            if ((err = ide_polling(channel, 1)))
            {
                log_warning("polling error: %x", err);
                return err;
            }
            insw(channels[channel].data_reg, buf, words);
            buf = ptr(addr(buf) + (words * 2));
        }
    }
    else
    {
        // PIO Write.
        for (i = 0; i < numsects; i++)
        {
            ide_polling(channel, 0); // Polling.
            asm("rep outsw" :: "c" (words), "d" (channels[channel].data_reg), "S" (buf)); // Send Data
            buf = ptr(addr(buf) + (words * 2));
        }

        ide_write(
            channel,
            ATA_REG_COMMAND,
            (char[]){ATA_CMD_CACHE_FLUSH, ATA_CMD_CACHE_FLUSH, ATA_CMD_CACHE_FLUSH_EXT}[lba_mode]);

        ide_polling(channel, 0); // Polling.
    }

    return 0;
}

static void ide_irq()
{
    uint8_t error, ata_status;
    uint8_t bm_status = inb(channels[0].bmide + 0x2);

    if (!(bm_status & BM_STATUS_INTERRUPT))
    {
        log_warning("not for this drive");
        return;
    }

    io_delay(10);
    if ((ata_status = ide_read(0, ATA_REG_STATUS)) & ATA_SR_ERR)
    {
        error = inb(ERROR_REG(0));
        log_warning("irq: error %x (%s)", error, ide_error_string(error));
    }

    /*if (bm_status & BM_STATUS_ACTIVE)*/
    /*{*/
        /*log_warning("status has BM_STATUS_ACTIVE");*/
        /*return;*/
    /*}*/

    io_delay(10);
    outb(0, channels[0].bmide);

    struct process* temp = wait_queue_front(&ide_queue);

    if (unlikely(!temp))
    {
        log_warning("no process in wq");
        return;
    }

    log_info("waking %u, bm status: %x, ata status: %x", temp->pid, bm_status, ata_status);
    process_wake(temp);
}

static int ide_fs_open(file_t* file)
{
    int drive = BLK_DRIVE(MINOR(file->inode->dev));
    if (!ide_devices[drive].mbr)
    {
        return -ENODEV;
    }
    return 0;
}

static int ide_fs_read(file_t* file, char* buf, size_t count)
{
    int errno;
    flags_t flags = 0;
    size_t sectors = count / ATA_SECTOR_SIZE;
    int drive = BLK_DRIVE(MINOR(file->inode->dev));
    int partition = BLK_PARTITION(MINOR(file->inode->dev));
    uint32_t offset = file->offset / ATA_SECTOR_SIZE;
    uint32_t first_sector, last_sector, max_count;

    if (partition != BLK_NO_PARTITION)
    {
        first_sector = ide_devices[drive].mbr->partitions[partition].lba_start;
        offset += first_sector;
        last_sector = first_sector + ide_devices[drive].mbr->partitions[partition].sectors;
    }
    else
    {
        last_sector = ide_devices[drive].size;
    }

    max_count = last_sector - offset;
    count = count > max_count
        ? max_count
        : count;

    if (ide_devices[drive].dma)
    {
        irq_save(flags);
    }

    ide_ata_access(ATA_READ, drive, offset, sectors, buf, ide_devices[drive].dma);

    if (ide_devices[drive].dma)
    {
        log_info("putting %u to sleep", process_current->pid);
        WAIT_QUEUE_DECLARE(q, process_current);
        if ((errno = process_wait_locked(&ide_queue, &q, flags)))
        {
            return errno;
        }
        memcpy(buf, dma_region->buffer, count);
    }

    file->offset += count;

    return count;
}
