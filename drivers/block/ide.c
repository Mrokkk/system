#define log_fmt(fmt) "ide: " fmt
#include "ide.h"

#include <arch/io.h>
#include <arch/dma.h>
#include <arch/pci.h>
#include <kernel/fs.h>
#include <kernel/irq.h>
#include <kernel/mbr.h>
#include <kernel/wait.h>
#include <kernel/devfs.h>
#include <kernel/mutex.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/process.h>
#include <kernel/byteorder.h>
#include <kernel/page_mmio.h>
#include <kernel/page_alloc.h>

#include "ata.h"
#include "scsi.h"

#define FORCE_PIO                   0
#define DISABLE_DMA_AFTER_FAILURE   1
#define IDE_POLLING_TIMEOUT_MS      1000
#define IDE_IO_DELAY                2

#define execute(operation, fail_string) \
    ({ int ret = operation; if (ret) { log_warning("%s: failed with %d", fail_string, ret);  }; ret; })

#define execute_no_ret(operation) \
    ({ operation; 0; })

static int ide_fs_open(file_t* file);
static int ide_fs_read(file_t* file, char* buf, size_t count);
static void ide_irq();
static void ide_write(uint8_t channel, uint8_t reg, uint8_t data);
static uint8_t ide_polling(uint8_t channel);
static int ide_pio_request(request_t* req);
static int ide_dma_request(request_t* req);
static int ide_atapi_request(request_t* req);

static pci_device_t* ide_pci;
static ide_channel_t channels[ATA_CHANNELS_SIZE];
static ata_device_t* ide_devices;
static uint8_t* ide_buf;
static dma_t* dma_region;

module_init(ide_initialize);
module_exit(ide_deinit);
KERNEL_MODULE(ide);

static file_operations_t ops = {
    .open = &ide_fs_open,
    .read = &ide_fs_read,
};

static void ide_wait(void)
{
    io_delay(IDE_IO_DELAY);
}

static uint8_t ide_read(uint8_t channel, uint8_t reg)
{
    return inb(channels[channel].base + reg);
}

static void ide_write(uint8_t channel, uint8_t reg, uint8_t data)
{
    log_debug(DEBUG_IDE, "outb(data=%x, port=%x)", data, channels[channel].base + reg);
    outb(data, channels[channel].base + reg);
}

static void ide_read_buffer(uint8_t channel, uint8_t reg, void* buffer, uint32_t count)
{
    insl(channels[channel].base + reg, buffer, count);
}

static void ide_enable_irq(uint8_t channel)
{
    outb(0x00, channels[channel].ctrl + ATA_REG_CONTROL);
}

static void ide_disable_irq(uint8_t channel)
{
    outb(0x02, channels[channel].ctrl + ATA_REG_CONTROL);
}

static uint8_t bm_readb(uint8_t channel, uint8_t reg)
{
    return inb(channels[channel].bmide + reg);
}

static void bm_writeb(uint8_t channel, uint8_t reg, uint8_t data)
{
    log_debug(DEBUG_IDE, "outb(data=%x, port=%x)", data, channels[channel].bmide + reg);
    outb(data, channels[channel].bmide + reg);
}

static void bm_writel(uint8_t channel, uint8_t reg, uint32_t data)
{
    log_debug(DEBUG_IDE, "outl(data=%x, reg=%x)", data, channels[channel].bmide + reg);
    outl(data, channels[channel].bmide + reg);
}

static void ide_channel_fill(int channel, pci_device_t* ide_pci)
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
    channels[channel].bmide = ide_pci->bar[4].addr & ~1;
    wait_queue_head_init(&channels[channel].queue);
}

static const char* ide_error_string(int e)
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

static void ide_device_atapi(int device)
{
    uint32_t channel  = device / 2;
    uint32_t slavebit = device % 2;
    uint32_t bus      = channels[channel].base;
    uint32_t count    = ATAPI_SECTOR_SIZE;
    uint8_t err;
    scsi_packet_t packet = ATAPI_READ_PACKET(0, 1);

    ide_write(channel, ATA_REG_HDDEVSEL, slavebit << 4);
    ide_wait();
    ide_write(channel, ATA_REG_FEATURES, 0); // PIO mode
    ide_write(channel, ATA_REG_LBA1, count & 0xFF);
    ide_write(channel, ATA_REG_LBA2, count >> 8);
    ide_write(channel, ATA_REG_COMMAND, ATA_CMD_PACKET);

    if ((ide_polling(channel)))
    {
        log_warning("channel %u: polling error");
        return;
    }

    outsw(bus, &packet, 6);
    ide_wait();

    page_t* page = page_alloc(1, 0);

    if (unlikely(!page))
    {
        log_warning("channel %u: cannot allocate buffer", channel);
        return;
    }

    void* buf = page_virt_ptr(page);

    if ((err = ide_polling(channel)))
    {
        goto finish;
    }

    insw(bus, buf, count / 2);

    memcpy(ide_buf, buf, ATA_SECTOR_SIZE);

finish:
    pages_free(page);
}

static void ide_device_register(ata_device_t* device)
{
    char buf[12];
    partition_t* p;
    mbr_t* mbr = ptr(ide_buf);
    char drive_dev_name[8];
    int device_id = device->id;

    ata_device_print(device);

    devfs_blk_register(
        ssnprintf(drive_dev_name, sizeof(drive_dev_name), "hd%c", device_id + 'a'),
        MAJOR_BLK_IDE,
        BLK_MINOR_DRIVE(device_id),
        &ops);

    if (device->type == ATA_TYPE_ATA)
    {
        request_t req = {
            .drive = device_id,
            .direction = ATA_READ,
            .offset = 0,
            .sectors = 1,
            .count = ATA_SECTOR_SIZE,
            .buffer = (char*)ide_buf,
            .dma = false
        };

        if (ide_pio_request(&req))
        {
            log_warning("error reading MBR");
            return;
        }
    }
    else
    {
        // FIXME: if I comment this, reads are not working
        ide_device_atapi(device->id);
    }

    log_info("  mbr: id = %x, signature = %x", mbr->id, mbr->signature);

    if (mbr->signature != MBR_SIGNATURE)
    {
        return;
    }

    memcpy(&device->mbr, mbr, ATA_SECTOR_SIZE);

    for (int part_id = 0; part_id < 4; ++part_id)
    {
        p = mbr->partitions + part_id;

        if (!p->lba_start)
        {
            continue;
        }

        log_info("  mbr: part[%u]:%x: [%08x - %08x]",
            part_id, p->type, p->lba_start, p->lba_start + p->sectors);

        devfs_blk_register(
            ssnprintf(buf, sizeof(buf), "%s%u", drive_dev_name, part_id),
            MAJOR_BLK_IDE,
            BLK_MINOR(part_id, device_id),
            &ops);
    }
}

static void ide_device_detect(int drive, bool use_dma)
{
    uint8_t channel = drive / 2;
    uint8_t role = drive % 2;
    bool atapi = false;
    uint8_t ch, cl, bm_status;

    ide_write(channel, ATA_REG_HDDEVSEL, 0xa0 | (role << 4));
    ide_wait();
    ide_write(channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ide_wait();

    if (ide_read(channel, ATA_REG_STATUS) == 0)
    {
        return;
    }

    if (ide_polling(channel))
    {
        cl = ide_read(channel, ATA_REG_LBA1);
        ch = ide_read(channel, ATA_REG_LBA2);

        if ((cl == 0x14 && ch == 0xeb) || (cl == 0x69 && ch == 0x96))
        {
            atapi = true;
            use_dma = false;
            ide_write(channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
            ide_wait();
        }
        else
        {
            return;
        }
    }

    memset(ide_buf, 0, ATA_IDENT_SIZE);

    ide_read_buffer(channel, ATA_REG_DATA, ide_buf, ATA_IDENT_SIZE);

    bm_status = bm_readb(channel, BM_REG_STATUS);

    ata_device_initialize(&ide_devices[drive], ide_buf, drive, NULL);

    if (atapi)
    {
        ide_devices[drive].type = ATA_TYPE_ATAPI;
        ide_devices[drive].size = 0;
        ide_devices[drive].signature = 1; // FIXME: hack
        ide_enable_irq(channel);
    }

    if ((ide_devices[drive].dma = use_dma))
    {
        bm_writeb(channel, BM_REG_STATUS, bm_status | BM_STATUS_INTERRUPT | BM_STATUS_DRV0_DMA | BM_STATUS_DRV1_DMA);
        ide_enable_irq(channel);
    }
}

static int ide_pci_interface_check(void)
{
    if (ide_pci->prog_if & 0x01)
    {
        log_warning("PCI native mode not supported yet");
        return -ENODEV;
    }

    return 0;
}

static void ide_pci_bm_initialize(bool* use_dma)
{
#if FORCE_PIO
    *use_dma = false;
#else
    if (!(ide_pci->prog_if & 0x80))
    {
        log_info("bus mastering is not supported");
        *use_dma = false;
    }

    dma_region = mmio_map_uc(DMA_PRD, DMA_SIZE, "dma");

    if (unlikely(!dma_region))
    {
        log_warning("cannot allocate DMA region");
        *use_dma = false;
    }
    else
    {
        memset(dma_region, 0, DMA_SIZE);
    }
#endif
}

static void ide_channels_initialize(void)
{
    ide_channel_fill(ATA_PRIMARY, ide_pci);
    ide_channel_fill(ATA_SECONDARY, ide_pci);

    ide_disable_irq(ATA_PRIMARY);
    ide_disable_irq(ATA_SECONDARY);
}

static void ide_devices_detect(bool use_dma)
{
    for (int i = 0; i < 4; i++)
    {
        ide_device_detect(i, use_dma);

        if (ide_devices[i].signature)
        {
            ide_device_register(&ide_devices[i]);
        }
    }
}

int ide_initialize(void)
{
    bool use_dma = true;

    if (!(ide_pci = pci_device_get(0x01, 0x01)))
    {
        log_notice("no IDE controller");
        return 0;
    }

    if (unlikely(!(ide_devices = single_page())))
    {
        return -ENOMEM;
    }

    ide_buf = shift_as(void*, ide_devices, PAGE_SIZE - ATA_SECTOR_SIZE);
    memset(ide_devices, 0, PAGE_SIZE);

    pci_device_print(ide_pci);

    if (execute(ide_pci_interface_check(), "checking prog if") ||
        execute(pci_device_initialize(ide_pci), "initializing PCI") ||
        execute_no_ret(ide_pci_bm_initialize(&use_dma)) ||
        execute_no_ret(ide_channels_initialize()) ||
        execute_no_ret(ide_devices_detect(use_dma)))
    {
        log_warning("cannot initialize IDE controller");
        return -EIO;
    }

    irq_register(14, &ide_irq, "ata1", IRQ_DEFAULT);
    irq_register(15, &ide_irq, "ata2", IRQ_DEFAULT);

    return 0;
}

int ide_deinit(void)
{
    return 0;
}

static uint8_t ide_polling(uint8_t channel)
{
    uint8_t status;

    ide_wait();

    size_t time_elapsed = 0;
    while (1)
    {
        status = ide_read(channel, ATA_REG_STATUS);
        if (time_elapsed > IDE_POLLING_TIMEOUT_MS)
        {
            log_warning("channel %u: timeout, status: %x", channel, status);
            return 2;
        }
        if (status & ATA_SR_ERR)
        {
            return 2;
        }
        else if (status & ATA_SR_DF)
        {
            return 1;
        }
        else if ((status & (ATA_SR_DRQ | ATA_SR_BSY)) == ATA_SR_DRQ)
        {
            break;
        }
        udelay(1000);
        ++time_elapsed;
    }

    return 0;
}

enum mode
{
    IDE_CHS = 0,
    IDE_LBA28 = 1,
    IDE_LBA48 = 2,
};

static int ide_ata_prepare_transfer(request_t* req)
{
    int lba_mode;
    uint8_t cmd, head, sect, lba_io[6];
    uint16_t cyl;
    uint32_t lba = req->offset;
    uint8_t channel = ide_devices[req->drive].id / 2;
    uint8_t slavebit = ide_devices[req->drive].id % 2;
    int direction = req->direction;
    bool dma = req->dma;

    if (lba >= 0x10000000) // LBA48
    {
        lba_mode  = IDE_LBA48;
        lba_io[0] = (lba & 0x000000ff) >> 0;
        lba_io[1] = (lba & 0x0000ff00) >> 8;
        lba_io[2] = (lba & 0x00ff0000) >> 16;
        lba_io[3] = (lba & 0xff000000) >> 24;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head      = 0; // Lower 4-bits of HDDEVSEL are not used here
    }
    else if (ide_devices[req->drive].capabilities & 0x200) // LBA28
    {
        lba_mode  = IDE_LBA28;
        lba_io[0] = (lba & 0x00000ff) >> 0;
        lba_io[1] = (lba & 0x000ff00) >> 8;
        lba_io[2] = (lba & 0x0ff0000) >> 16;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head      = (lba & 0xf000000) >> 24;
    }
    else // CHS
    {
        lba_mode  = IDE_CHS;
        sect      = (lba % 63) + 1;
        cyl       = (lba + 1  - sect) / (16 * 63);
        lba_io[0] = sect;
        lba_io[1] = (cyl >> 0) & 0xff;
        lba_io[2] = (cyl >> 8) & 0xff;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head      = (lba + 1 - sect) % (16 * 63) / (63);
    }

    while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY);

    if (lba_mode == IDE_CHS)
    {
        ide_write(channel, ATA_REG_HDDEVSEL, 0xa0 | (slavebit << 4) | head);
    }
    else
    {
        ide_write(channel, ATA_REG_HDDEVSEL, 0xe0 | (slavebit << 4) | head);
    }

    if (lba_mode == IDE_LBA48)
    {
        ide_write(channel, ATA_REG_SECCOUNT1,   0);
        ide_write(channel, ATA_REG_LBA3,        lba_io[3]);
        ide_write(channel, ATA_REG_LBA4,        lba_io[4]);
        ide_write(channel, ATA_REG_LBA5,        lba_io[5]);
    }

    ide_write(channel, ATA_REG_SECCOUNT0,   req->sectors);
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

static int ide_pio_request(request_t* req)
{
    int lba_mode;
    uint32_t channel = ide_devices[req->drive].id / 2;
    uint32_t words = ATA_SECTOR_SIZE / 2;
    uint8_t err;
    char* buf = req->buffer;

    lba_mode = ide_ata_prepare_transfer(req);

    if (req->direction == ATA_READ)
    {
        for (int i = 0; i < req->sectors; i++)
        {
            if ((err = ide_polling(channel)))
            {
                log_warning("polling error: %x", err);
                return -EIO;
            }
            insw(channels[channel].data_reg, buf, words);
            buf = shift(buf, words * 2);
        }
    }
    else
    {
        for (int i = 0; i < req->sectors; i++)
        {
            ide_polling(channel);
            outsw(channels[channel].data_reg, buf, words);
            buf = shift(buf, words * 2);
        }

        ide_write(
            channel,
            ATA_REG_COMMAND,
            (char[]){ATA_CMD_CACHE_FLUSH, ATA_CMD_CACHE_FLUSH, ATA_CMD_CACHE_FLUSH_EXT}[lba_mode]);

        ide_polling(channel);
    }

    ide_read(channel, ATA_REG_STATUS);

    return 0;
}

static int ide_dma_request(request_t* req)
{
    int errno;
    flags_t flags;
    uint32_t channel = ide_devices[req->drive].id / 2;
    request_t** current_request = &channels[channel].current_request;

    irq_save(flags);

    // FIXME: currently nothing blocks signals when drive is accessed, so it's possible to kill
    // the process before it manages to clear the request
    if (unlikely(*current_request))
    {
        log_warning("cannot perform DMA request, another one is ongoing: %x", current_request);
        errno = -EAGAIN;
        goto error;
    }

    if (unlikely(!(*current_request = alloc(request_t, memcpy(this, req, sizeof(*req))))))
    {
        log_warning("no memory to allocate DMA request");
        errno = -ENOMEM;
        goto error;
    }

    // 1) Software prepares a PRD Table in system memory. Each PRD is 8 bytes long and consists of an
    // address pointer to the starting address and the transfer count of the memory buffer to be
    // transferred. In any given PRD Table, two consecutive PRDs are offset by 8-bytes and are aligned
    // on a 4-byte boundary.
    dma_region->prdt->addr = DMA_START;
    dma_region->prdt->count = req->sectors * ATA_SECTOR_SIZE | DMA_EOT;

    log_debug(DEBUG_IDE, "dma: ch%u buffer: {phys=%x, virt=%x}, PRD: addr: %x, size: %x",
        channel,
        DMA_START,
        dma_region->buffer,
        dma_region->prdt->addr,
        dma_region->prdt->count);

    // 2) Software provides the starting address of the PRD Table by loading the PRD Table Pointer
    // Register. The direction of the data transfer is specified by setting the Read/Write Control bit.
    // Clear the Interrupt bit and Error bit in the Status register.
    bm_writel(channel, BM_REG_PRDT, DMA_PRD);
    bm_writeb(channel, BM_REG_CMD, BM_CMD_READ);
    bm_writeb(channel, BM_REG_STATUS, BM_STATUS_ERROR | BM_STATUS_INTERRUPT);

    // 3) Software issues the appropriate DMA transfer command to the disk device.
    ide_ata_prepare_transfer(req);

    // 4) Engage the bus master function by writing a '1' to the Start bit in the Bus Master IDE Command
    // Register for the appropriate channel.
    bm_writeb(channel, BM_REG_CMD, BM_CMD_READ | BM_CMD_START);

    log_debug(DEBUG_IDE, "putting %u to sleep", process_current->pid);

    WAIT_QUEUE_DECLARE(q, process_current);

    if ((errno = process_wait_locked(&channels[channel].queue, &q, &flags)))
    {
        goto cleanup_request;
    }

    if (unlikely((errno = (*current_request)->errno)))
    {
        goto cleanup_request;
    }

    delete(*current_request);
    *current_request = NULL;
    log_debug(DEBUG_IDE, "copying %u B from %x to %x", req->count, dma_region->buffer, req->buffer);
    memcpy(req->buffer, dma_region->buffer, req->count);
    irq_restore(flags);

    return 0;

cleanup_request:
    delete(*current_request);
    *current_request = NULL;
    if (DISABLE_DMA_AFTER_FAILURE)
    {
        req->dma = false;
        irq_restore(flags);
        return ide_pio_request(req);
    }
error:
    irq_restore(flags);
    return errno;
}

static int ide_atapi_request(request_t* req)
{
    int errno;
    flags_t flags;
    uint32_t channel  = req->drive / 2;
    uint32_t slavebit = req->drive % 2;
    uint32_t bus      = channels[channel].base;
    uint32_t count    = req->count;
    // Request contains offset in sectors of ATA_SECTOR_SIZE
    int lba = req->offset / (ATAPI_SECTOR_SIZE / ATA_SECTOR_SIZE);
    int numsects = count / ATAPI_SECTOR_SIZE;
    scsi_packet_t packet = ATAPI_READ_PACKET(lba, numsects);
    request_t** current_request = &channels[channel].current_request;

    if (count % ATAPI_SECTOR_SIZE)
    {
        log_warning("invalid count: %u", count);
        return -EINVAL;
    }

    irq_save(flags);

    log_debug(DEBUG_IDE, "count: %u B, sectors: %u, lba: %u", count, numsects, lba);

    if (unlikely(*current_request))
    {
        log_warning("cannot perform DMA request, another one is ongoing: %x", *current_request);
        errno = -EAGAIN;
        goto error;
    }

    if (unlikely(!(*current_request = alloc(request_t, memcpy(this, req, sizeof(*req))))))
    {
        log_warning("no memory to allocate DMA request");
        errno = -ENOMEM;
        goto error;
    }

    ide_write(channel, ATA_REG_HDDEVSEL, slavebit << 4);
    ide_wait();
    ide_write(channel, ATA_REG_FEATURES, 0); // PIO mode
    ide_write(channel, ATA_REG_LBA1, count & 0xff);
    ide_write(channel, ATA_REG_LBA2, count >> 8);
    ide_write(channel, ATA_REG_COMMAND, ATA_CMD_PACKET);

    if ((ide_polling(channel)))
    {
        log_warning("channel %u: polling error");
        errno = -EIO;
        goto error;
    }

    outsw(bus, &packet, 6);

    log_debug(DEBUG_IDE, "putting %u to sleep", process_current->pid);
    WAIT_QUEUE_DECLARE(q, process_current);

    if ((errno = process_wait_locked(&channels[channel].queue, &q, &flags)))
    {
        goto cleanup_request;
    }

    if (unlikely((errno = (*current_request)->errno)))
    {
        goto cleanup_request;
    }

    char* buf = req->buffer;
    for (int i = 0; i < numsects; i++)
    {
        if ((ide_polling(channel)))
        {
            errno = -EIO;
            goto error;
        }
        insw(bus, buf, ATAPI_SECTOR_SIZE / 2);
        buf += ATAPI_SECTOR_SIZE;
    }

    log_debug(DEBUG_IDE, "putting %u to sleep", process_current->pid);
    if ((errno = process_wait_locked(&channels[channel].queue, &q, &flags)))
    {
        goto cleanup_request;
    }

    ide_wait();
    while (ide_read(channel, ATA_REG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ));

    delete(*current_request);
    *current_request = NULL;

    irq_restore(flags);

    return 0;

cleanup_request:
    delete(*current_request);
    *current_request = NULL;
error:
    irq_restore(flags);
    return errno;
}

static int ide_atapi_read_capacity(ata_device_t* device)
{
    int errno;
    flags_t flags;
    uint32_t channel  = device->id / 2;
    uint32_t slavebit = device->id % 2;
    uint32_t bus      = channels[channel].base;
    u32_msb_t buffer[2];
    scsi_packet_t packet = ATAPI_READ_CAPACITY_PACKET();
    request_t** current_request = &channels[channel].current_request;

    irq_save(flags);

    memset(buffer, 0, sizeof(buffer));

    if (unlikely(*current_request))
    {
        log_warning("cannot perform DMA request, another one is ongoing: %x", *current_request);
        errno = -EAGAIN;
        goto error;
    }

    if (unlikely(!(*current_request = alloc(request_t, memset(this, 0, sizeof(*this))))))
    {
        log_warning("no memory to allocate DMA request");
        errno = -ENOMEM;
        goto error;
    }

    (*current_request)->drive = device->id;

    ide_write(channel, ATA_REG_HDDEVSEL, slavebit << 4);
    ide_wait();
    ide_write(channel, ATA_REG_FEATURES, 0); // PIO mode
    ide_write(channel, ATA_REG_LBA1, 8);
    ide_write(channel, ATA_REG_LBA2, 8);
    ide_write(channel, ATA_REG_COMMAND, ATA_CMD_PACKET);

    if ((ide_polling(channel)))
    {
        log_warning("channel %u: polling error");
        errno = -EIO;
        goto error;
    }

    outsw(bus, &packet, 6);

    log_debug(DEBUG_IDE, "putting %u to sleep", process_current->pid);
    WAIT_QUEUE_DECLARE(q, process_current);

    if ((errno = process_wait_locked(&channels[channel].queue, &q, &flags)))
    {
        goto cleanup_request;
    }

    if (unlikely((errno = (*current_request)->errno)))
    {
        goto cleanup_request;
    }

    if ((ide_polling(channel)))
    {
        errno = -EIO;
        goto error;
    }

    insw(bus, buffer, 4);

    log_debug(DEBUG_IDE, "putting %u to sleep", process_current->pid);
    if ((errno = process_wait_locked(&channels[channel].queue, &q, &flags)))
    {
        goto cleanup_request;
    }

    while (ide_read(channel, ATA_REG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ));

    delete(*current_request);
    *current_request = NULL;

    uint32_t size = (NATIVE_U32(buffer[0]) + 1) * NATIVE_U32(buffer[1]);

    device->size = size / ATA_SECTOR_SIZE;

    char* unit;
    human_size(size, unit);
    log_debug(DEBUG_IDE, "ATAPI drive capacity: %u %s", size, unit);

    irq_restore(flags);

    return 0;

cleanup_request:
    delete(*current_request);
    *current_request = NULL;
error:
    irq_restore(flags);
    return errno;
}

static void ide_irq(int nr)
{
    int errno = 0;
    uint8_t error, ata_status;
    int channel = nr == 14 ? ATA_PRIMARY : ATA_SECONDARY;
    uint8_t bm_status = bm_readb(channel, BM_REG_STATUS);
    request_t* current_request = channels[channel].current_request;

    if (unlikely(!current_request))
    {
        log_warning("IRQ%u: channel %u without ongoing request", nr, channel);
        return;
    }

    if (unlikely(ide_devices[current_request->drive].dma && !(bm_status & BM_STATUS_INTERRUPT)))
    {
        log_warning("IRQ%u: channel %u without proper BM status; status: %x", nr, channel, bm_status);
    }

    if (unlikely((ata_status = ide_read(channel, ATA_REG_STATUS)) & ATA_SR_ERR))
    {
        error = ide_read(channel, ATA_REG_ERROR);
        log_warning("irq: error %x (%s)", error, ide_error_string(error));
        errno = -EIO;
    }

    if (unlikely(bm_status & BM_STATUS_ACTIVE))
    {
        log_warning("DMA request not finished, error: %x, command: %x", ide_pci->status, ide_pci->command);
        errno = -EIO;
    }

    // 6) At the end of the transfer the IDE device signals an interrupt.
    // 7) In response to the interrupt, software resets the Start/Stop bit in the command register. It then
    // reads the controller status and then the drive status to determine if the transfer completed
    // successfully.

    bm_writeb(channel, BM_REG_CMD, 0);

    process_t* proc = wait_queue_pop(&channels[channel].queue);

    if (unlikely(!proc))
    {
        log_warning("no process in wq");
        return;
    }

    if (DISABLE_DMA_AFTER_FAILURE && (current_request->errno = errno))
    {
        log_warning("IRQ%u: disabling DMA for drive %u", nr, current_request->drive);
        ide_devices[current_request->drive].dma = false;
        bm_writeb(channel, BM_REG_STATUS, bm_status | BM_STATUS_INTERRUPT);
        ide_disable_irq(channel);
    }

    log_debug(DEBUG_IDE, "waking %u, bm status: %x, ata status: %x", proc->pid, bm_status, ata_status);
    process_wake(proc);
}

static int ide_fs_open(file_t* file)
{
    int drive = BLK_DRIVE(MINOR(file->dentry->inode->rdev));
    int partition = BLK_PARTITION(MINOR(file->dentry->inode->rdev));
    if (partition != BLK_NO_PARTITION && !ide_devices[drive].mbr.signature)
    {
        return -ENODEV;
    }
    return 0;
}

static request_t ide_request_create(int direction, file_t* file, char* buf, size_t count)
{
    int errno = 0;
    uint32_t first_sector, last_sector, max_count;
    size_t sectors      = count / ATA_SECTOR_SIZE;
    int drive           = BLK_DRIVE(MINOR(file->dentry->inode->rdev));
    int partition       = BLK_PARTITION(MINOR(file->dentry->inode->rdev));
    uint32_t offset     = file->offset / ATA_SECTOR_SIZE;
    mbr_t* mbr          = &ide_devices[drive].mbr;

    if (partition != BLK_NO_PARTITION)
    {
        first_sector = mbr->partitions[partition].lba_start;
        offset += first_sector;
        last_sector = first_sector + mbr->partitions[partition].sectors;
    }
    else
    {
        last_sector = ide_devices[drive].size;
        if (!last_sector && ide_devices[drive].type == ATA_TYPE_ATAPI)
        {
            ide_atapi_read_capacity(&ide_devices[drive]);
        }
        if (!(last_sector = ide_devices[drive].size))
        {
            log_warning("cannot determine drive %u size", drive);
            errno = -EAGAIN;
        }
    }

    max_count = (last_sector - offset) * ATA_SECTOR_SIZE;
    count = count > max_count
        ? max_count
        : count;

    log_debug(DEBUG_IDE, "drive=%x, dir=%u, offset=%x, sectors=%x, count=%x",
        drive,
        direction,
        offset,
        sectors,
        count);

    request_t req = {
        .drive = drive,
        .direction = direction,
        .offset = offset,
        .sectors = sectors,
        .count = count,
        .buffer = buf,
        .dma = ide_devices[drive].dma,
        .errno = errno
    };

    return req;
}

static int ide_request_handle(request_t* req)
{
    int errno;

    if (unlikely((errno = req->errno)))
    {
        return errno;
    }

    if (req->count == 0)
    {
        return 0;
    }

    if (ide_devices[req->drive].type == ATA_TYPE_ATAPI)
    {
        return ide_atapi_request(req);
    }

    return req->dma ? ide_dma_request(req) : ide_pio_request(req);
}

static int ide_fs_read(file_t* file, char* buf, size_t count)
{
    int errno;
    request_t req = ide_request_create(ATA_READ, file, buf, count);

    if ((errno = ide_request_handle(&req)))
    {
        return errno;
    }

    file->offset += req.count;

    return req.count;
}
