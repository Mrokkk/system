#define log_fmt(fmt) "ide: " fmt
#include "ide.h"

#include <arch/io.h>
#include <arch/dma.h>
#include <arch/pci.h>
#include <kernel/fs.h>
#include <kernel/irq.h>
#include <kernel/scsi.h>
#include <kernel/time.h>
#include <kernel/wait.h>
#include <kernel/devfs.h>
#include <kernel/mutex.h>
#include <kernel/blkdev.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/execute.h>
#include <kernel/process.h>
#include <kernel/byteorder.h>
#include <kernel/page_mmio.h>
#include <kernel/page_alloc.h>

#include "ata.h"

#define DEBUG_IDE                   0
#define FORCE_PIO                   0
#define DISABLE_DMA_AFTER_FAILURE   0
#define IDE_POLLING_TIMEOUT_MS      1000
#define IDE_IO_DELAY                2

static int ide_blk_read(void* blkdev, size_t offset, void* buffer, size_t size, bool irq);
static int ide_blk_medium_detect(void* blkdev, size_t* block_size, size_t* sectors);
static void ide_irq();
static void ide_write(uint8_t channel, uint8_t reg, uint8_t data);
static uint8_t ide_polling(uint8_t channel);
static int ide_pio_request(request_t* req);
static int ide_dma_request(request_t* req);

static pci_device_t* ide_pci;
static ide_channel_t channels[ATA_CHANNELS_SIZE];
static ata_device_t* ide_devices;
static uint8_t* ide_buf;
static dma_t* dma_region;

module_init(ide_initialize);
module_exit(ide_deinit);
KERNEL_MODULE(ide);

READONLY static blkdev_ops_t bops = {
    .read = &ide_blk_read,
    .medium_detect = &ide_blk_medium_detect,
};

static uint8_t ide_read(uint8_t channel, uint8_t reg)
{
    return inb(channels[channel].base + reg);
}

static void ide_write(uint8_t channel, uint8_t reg, uint8_t data)
{
    log_debug(DEBUG_IDE, "outb(data=%#x, port=%#x)", data, channels[channel].base + reg);
    outb(data, channels[channel].base + reg);
}

static void ide_buffer_read(uint8_t channel, uint8_t reg, void* buffer, uint32_t size)
{
    insw(channels[channel].base + reg, buffer, size / 2);
}

static void ide_buffer_write(uint8_t channel, uint8_t reg, const void* buffer, uint32_t size)
{
    outsw(channels[channel].base + reg, buffer, size / 2);
}

static uint8_t ide_status_read(uint8_t channel)
{
    return ide_read(channel, ATA_REG_STATUS);
}

static uint8_t ide_alt_status_read(uint8_t channel)
{
    return inb(channels[channel].ctrl + ATA_REG_ALTSTATUS);
}

static void ide_control_write(uint8_t channel, uint8_t data)
{
    outb(data, channels[channel].ctrl + ATA_REG_CONTROL);
}

static void ide_enable_irq(uint8_t channel)
{
    ide_control_write(channel, 0x00);
}

static void ide_disable_irq(uint8_t channel)
{
    ide_control_write(channel, 0x02);
}

static void ide_wait(uint8_t channel)
{
    ide_alt_status_read(channel);
    ide_alt_status_read(channel);
    ide_alt_status_read(channel);
    ide_alt_status_read(channel);
}

static uint8_t bm_readb(uint8_t channel, uint8_t reg)
{
    return inb(channels[channel].bmide + reg);
}

static void bm_writeb(uint8_t channel, uint8_t reg, uint8_t data)
{
    log_debug(DEBUG_IDE, "outb(data=%#x, port=%#x)", data, channels[channel].bmide + reg);
    outb(data, channels[channel].bmide + reg);
}

static void bm_writel(uint8_t channel, uint8_t reg, uint32_t data)
{
    log_debug(DEBUG_IDE, "outl(data=%#x, reg=%#x)", data, channels[channel].bmide + reg);
    outl(data, channels[channel].bmide + reg);
}

static void ide_channel_fill(int channel, pci_device_t* ide_pci)
{
    uint16_t base = channel ? 0x170 : 0x1f0;
    uint16_t ctrl = base + 0x206;

    if (ide_pci)
    {
        bar_t* cmd_bar = &ide_pci->bar[channel * 2];
        bar_t* ctrl_bar = &ide_pci->bar[channel * 2 + 1];
        if (cmd_bar->addr && cmd_bar->size && cmd_bar->region == PCI_IO)
        {
            base = cmd_bar->addr;
        }
        if (ctrl_bar->addr && ctrl_bar->size && ctrl_bar->region == PCI_IO)
        {
            ctrl = ctrl_bar->addr + 2;
        }
    }

    channels[channel].base  = base;
    channels[channel].ctrl  = ctrl;
    channels[channel].bmide = ide_pci ? ide_pci->bar[4].addr & ~1 : 0;
    mutex_init(&channels[channel].lock);
    wait_queue_head_init(&channels[channel].queue);
}

static const char* ide_error_string(int e)
{
    switch (e)
    {
        case 0:            return "No error";
        case ATA_ER_BBK:   return "Bad Block detected";
        case ATA_ER_UNC:   return "Uncorrectable data error";
        case ATA_ER_MC:    return "Media changed";
        case ATA_ER_IDNF:  return "ID not found";
        case ATA_ER_MCR:   return "Media change request";
        case ATA_ER_ABRT:  return "Aborted command";
        case ATA_ER_TK0NF: return "Track zero not found";
        case ATA_ER_AMNF:  return "Address mark not found";
        default:           return "Unknown error";
    }
}

static void ide_device_register(ata_device_t* device)
{
    int errno;
    int major;

    if (device->type == ATA_TYPE_ATAPI)
    {
        major = MAJOR_BLK_CDROM;
    }
    else
    {
        major = MAJOR_BLK_IDE;
    }

    blkdev_char_t blk = {
        .major = major,
        .model = device->model,
        .sectors = device->sectors,
        .block_size = device->sector_size,
    };

    if (unlikely(errno = blkdev_register(&blk, device, &bops)))
    {
        log_info("failed to register blk device");
    }
}

static int ide_ready_wait(uint8_t channel)
{
    size_t time_elapsed = 0;

    while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY)
    {
        udelay(10);
        if (time_elapsed++ > 100)
        {
            return 1;
        }
    }

    return 0;
}

static void ide_device_detect(int drive, bool use_dma)
{
    uint8_t channel = drive / 2;
    uint8_t role = drive % 2;
    bool atapi = false;
    uint8_t ch, cl;

    memset(ide_buf, 0, ATA_IDENT_SIZE);

    ide_write(channel, ATA_REG_HDDEVSEL, 0xa0 | (role << 4));
    ide_wait(channel);
    ide_write(channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ide_wait(channel);

    if (ide_ready_wait(channel))
    {
        log_warning("[drive %u] timeout; status: %#x", drive, ide_read(channel, ATA_REG_STATUS));
        return;
    }

    if (ide_status_read(channel) == 0)
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

            ide_write(channel, ATA_REG_HDDEVSEL, 0xa0 | (role << 4));
            ide_wait(channel);
            ide_write(channel, ATA_REG_FEATURES, 0);
            ide_wait(channel);
            ide_write(channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
            ide_wait(channel);

            if (ide_polling(channel))
            {
                log_warning("[drive %u] IDENTIFY PACKET failed");
                return;
            }
        }
        else
        {
            return;
        }
    }

    ide_buffer_read(channel, ATA_REG_DATA, ide_buf, ATA_IDENT_SIZE);

    if (ide_ready_wait(channel))
    {
        log_warning("[drive %u] busy; status: %#x", ide_status_read(channel));
    }

    ata_device_initialize(&ide_devices[drive], ide_buf, drive, NULL);

    if (atapi)
    {
        ide_devices[drive].type = ATA_TYPE_ATAPI;
        ide_enable_irq(channel);
    }

    if ((ide_devices[drive].dma = use_dma))
    {
        uint8_t bm_status = bm_readb(channel, BM_REG_STATUS);
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

static int isa_ide_controller_detect(void)
{
    outb(0xa0, ATA_REG_BASE + ATA_REG_HDDEVSEL);
    uint8_t status = inb(ATA_REG_BASE + ATA_REG_STATUS);

    if (!(status & ATA_SR_DRDY) || status == 0xff)
    {
        return -ENODEV;
    }

    return 0;
}

UNMAP_AFTER_INIT int ide_initialize(void)
{
    bool use_dma = true;

    if (!(ide_pci = pci_device_get(0x01, 0x01)))
    {
        log_notice("no PCI IDE controller");
        if (isa_ide_controller_detect())
        {
            log_notice("no ISA IDE controller");
            return 0;
        }
        else
        {
            log_notice("ISA IDE controller available");
        }
    }

    if (unlikely(!(ide_devices = single_page())))
    {
        return -ENOMEM;
    }

    ide_buf = shift_as(void*, ide_devices, PAGE_SIZE - MBR_SECTOR_SIZE);
    memset(ide_devices, 0, PAGE_SIZE);

    if (ide_pci)
    {
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
    }
    else
    {
        use_dma = false;
        ide_channels_initialize();
        ide_devices_detect(use_dma);
    }

    irq_register(14, &ide_irq, "ata1", IRQ_DEFAULT, NULL);
    irq_register(15, &ide_irq, "ata2", IRQ_DEFAULT, NULL);

    return 0;
}

int ide_deinit(void)
{
    return 0;
}

static uint8_t ide_polling(uint8_t channel)
{
    uint8_t status;
    size_t time_elapsed = 0;

    while (1)
    {
        status = ide_status_read(channel);
        if (time_elapsed > IDE_POLLING_TIMEOUT_MS * 100)
        {
            log_warning("channel %u: timeout, status: %#x", channel, status);
            return IDE_TIMEOUT;
        }
        else if (status & (ATA_SR_ERR | ATA_SR_DF))
        {
            return IDE_ERROR;
        }
        else if ((status & (ATA_SR_DRQ | ATA_SR_BSY)) == ATA_SR_DRQ)
        {
            break;
        }
        udelay(USEC_IN_MSEC / 100);
        ++time_elapsed;
    }

    return IDE_NO_ERROR;
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
    uint8_t channel = req->device->id / 2;
    uint8_t slavebit = req->device->id % 2;
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
    else if (req->device->capabilities & 0x200) // LBA28
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

    while (ide_status_read(channel) & ATA_SR_BSY);

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
        ide_write(channel, ATA_REG_SECCOUNT1, 0);
        ide_write(channel, ATA_REG_LBA3,      lba_io[3]);
        ide_write(channel, ATA_REG_LBA4,      lba_io[4]);
        ide_write(channel, ATA_REG_LBA5,      lba_io[5]);
    }

    ide_write(channel, ATA_REG_SECCOUNT0, req->sectors);
    ide_write(channel, ATA_REG_LBA0,      lba_io[0]);
    ide_write(channel, ATA_REG_LBA1,      lba_io[1]);
    ide_write(channel, ATA_REG_LBA2,      lba_io[2]);

    while (ide_status_read(channel) & ATA_SR_BSY);

    if (lba_mode == IDE_CHS   && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
    if (lba_mode == IDE_LBA28 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
    if (lba_mode == IDE_LBA48 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO_EXT;
    if (lba_mode == IDE_CHS   && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == IDE_LBA28 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == IDE_LBA48 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA_EXT;
    if (lba_mode == IDE_CHS   && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == IDE_LBA28 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == IDE_LBA48 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO_EXT;
    if (lba_mode == IDE_CHS   && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == IDE_LBA28 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == IDE_LBA48 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA_EXT;

    ide_write(channel, ATA_REG_COMMAND, cmd);

    return lba_mode;
}

static int ide_pio_request(request_t* req)
{
    int lba_mode;
    size_t sector_size = req->device->sector_size;
    uint32_t channel = req->device->id / 2;
    uint8_t err;
    void* buf = req->buffer;

    lba_mode = ide_ata_prepare_transfer(req);

    if (req->direction == ATA_READ)
    {
        for (int i = 0; i < req->sectors; i++)
        {
            if (unlikely(err = ide_polling(channel)))
            {
                log_warning("polling error: %#x", err);
                return -EIO;
            }
            ide_buffer_read(channel, ATA_REG_DATA, buf, sector_size);
            buf += sector_size;
        }
    }
    else
    {
        for (int i = 0; i < req->sectors; i++)
        {
            ide_polling(channel);
            ide_buffer_write(channel, ATA_REG_DATA, buf, sector_size);
            buf += sector_size;
        }

        ide_write(
            channel,
            ATA_REG_COMMAND,
            (char[]){ATA_CMD_CACHE_FLUSH, ATA_CMD_CACHE_FLUSH, ATA_CMD_CACHE_FLUSH_EXT}[lba_mode]);

        ide_polling(channel);
    }

    ide_status_read(channel);

    return 0;
}

static int ide_dma_request(request_t* req)
{
    int errno;
    flags_t flags;
    uint32_t channel = req->device->id / 2;
    request_t** current_request = &channels[channel].current_request;

    irq_save(flags);

    // FIXME: currently nothing blocks signals when drive is accessed, so it's possible to kill
    // the process before it manages to clear the request
    if (unlikely(*current_request))
    {
        log_warning("cannot perform DMA request, another one is ongoing: %#x", current_request);
        errno = -EAGAIN;
        goto error;
    }

    *current_request = req;

    // 1) Software prepares a PRD Table in system memory. Each PRD is 8 bytes long and consists of an
    // address pointer to the starting address and the transfer count of the memory buffer to be
    // transferred. In any given PRD Table, two consecutive PRDs are offset by 8-bytes and are aligned
    // on a 4-byte boundary.
    dma_region->prdt->addr = DMA_START;
    dma_region->prdt->count = req->sectors * req->device->sector_size | DMA_EOT;

    log_debug(DEBUG_IDE, "dma: ch%u buffer: {phys=%#x, virt=%#x}, PRD: addr: %#x, size: %#x",
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

    if (unlikely(errno = process_wait_locked(&channels[channel].queue, &q, &flags)))
    {
        goto cleanup_request;
    }

    if (unlikely(errno = req->errno))
    {
        goto cleanup_request;
    }

    *current_request = NULL;
    log_debug(DEBUG_IDE, "copying %u B from %#x to %#x", req->count, dma_region->buffer, req->buffer);
    memcpy(req->buffer, dma_region->buffer, req->count);
    irq_restore(flags);

    return 0;

cleanup_request:
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

static int ide_atapi_scsi_command(ata_device_t* device, scsi_packet_t* packet, void* data, size_t size)
{
    int errno;
    flags_t flags;
    uint32_t channel   = device->id / 2;
    uint32_t slavebit  = device->id % 2;
    size_t sector_size = device->sector_size ? device->sector_size : ATAPI_SECTOR_SIZE;
    request_t** current_request = &channels[channel].current_request;
    request_t req = {.device = device};

    if (unlikely(size > 0xffff))
    {
        return -EINVAL;
    }

    irq_save(flags);

    if (unlikely(*current_request))
    {
        log_warning("cannot perform request, another one is ongoing: %#x", *current_request);
        errno = -EAGAIN;
        goto error;
    }

    *current_request = &req;

    ide_write(channel, ATA_REG_HDDEVSEL, slavebit << 4);
    ide_write(channel, ATA_REG_FEATURES, 0);
    ide_write(channel, ATA_REG_LBA1, size & 0xff);
    ide_write(channel, ATA_REG_LBA2, size >> 8);
    ide_write(channel, ATA_REG_COMMAND, ATA_CMD_PACKET);

    if (unlikely(ide_polling(channel)))
    {
        log_warning("channel %u: polling error");
        errno = -EIO;
        goto error;
    }

    ide_buffer_write(channel, ATA_REG_DATA, packet, sizeof(*packet));

    log_debug(DEBUG_IDE, "putting %u to sleep", process_current->pid);
    WAIT_QUEUE_DECLARE(q, process_current);

    if (unlikely(errno = process_wait_locked(&channels[channel].queue, &q, &flags)))
    {
        goto cleanup_request;
    }

    if (unlikely(errno = req.errno))
    {
        goto cleanup_request;
    }

    uint8_t* buf = data;
    for (; size;)
    {
        size_t packet_size = size > sector_size ? sector_size : size;
        if (unlikely(ide_polling(channel)))
        {
            errno = -EIO;
            goto cleanup_request;
        }
        ide_buffer_read(channel, ATA_REG_DATA, buf, packet_size);
        buf += packet_size;
        size -= packet_size;
    }

    log_debug(DEBUG_IDE, "putting %u to sleep", process_current->pid);
    if (unlikely(errno = process_wait_locked(&channels[channel].queue, &q, &flags)))
    {
        goto cleanup_request;
    }

    while (ide_read(channel, ATA_REG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ));

    *current_request = NULL;

    irq_restore(flags);

    return 0;

cleanup_request:
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
        ide_read(channel, ATA_REG_STATUS);
        bm_writeb(channel, BM_REG_CMD, 0);
        return;
    }

    if (unlikely(current_request->device->dma && !(bm_status & BM_STATUS_INTERRUPT)))
    {
        log_warning("IRQ%u: channel %u without proper BM status; status: %#x", nr, channel, bm_status);
    }

    if (unlikely((ata_status = ide_read(channel, ATA_REG_STATUS)) & ATA_SR_ERR))
    {
        error = ide_read(channel, ATA_REG_ERROR);
        log_info("irq: error %#x (%s)", error, ide_error_string(error));
        errno = -EIO;
    }

    if (unlikely(bm_status & BM_STATUS_ACTIVE))
    {
        log_warning("DMA request not finished, error: %#x, command: %#x", ide_pci->status, ide_pci->command);
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
        log_warning("IRQ%u: disabling DMA for drive %u", nr, current_request->device->id);
        current_request->device->dma = false;
        bm_writeb(channel, BM_REG_STATUS, bm_status | BM_STATUS_INTERRUPT);
        ide_disable_irq(channel);
    }

    log_debug(DEBUG_IDE, "waking %u, bm status: %#x, ata status: %#x", proc->pid, bm_status, ata_status);
    process_wake(proc);
}

static int ide_request_handle(request_t* req, bool irq)
{
    if (req->count == 0)
    {
        return 0;
    }

    uint32_t channel = req->device->id / 2;

    scoped_mutex_lock(&channels[channel].lock);

    if (req->device->type == ATA_TYPE_ATAPI)
    {
        if (!irq)
        {
            log_warning("%s: ATAPI requires IRQ enabled", __func__);
            return -EINVAL;
        }
        scsi_packet_t packet = SCSI_READ_PACKET(req->offset, req->sectors);
        return ide_atapi_scsi_command(req->device, &packet, req->buffer, req->count);
    }

    return req->dma ? ide_dma_request(req) : ide_pio_request(req);
}

static int ide_blk_read(void* blkdev, size_t offset, void* buffer, size_t sectors, bool irq)
{
    int errno;
    ata_device_t* device = blkdev;
    size_t sector_size = device->sector_size;

    request_t req = {
        .device = device,
        .direction = ATA_READ,
        .offset = offset,
        .sectors = sectors,
        .count = sectors * sector_size,
        .buffer = buffer,
        .dma = device->dma && irq,
    };

    if (unlikely(errno = ide_request_handle(&req, irq)))
    {
        return errno;
    }

    return 0;
}

static int ide_blk_medium_detect(void* blkdev, size_t* block_size, size_t* sectors)
{
    int errno;
    ata_device_t* device = blkdev;

    if (device->type != ATA_TYPE_ATAPI)
    {
        return -ENOSYS;
    }

    scsi_packet_t packet = SCSI_READ_CAPACITY_PACKET();
    scsi_capacity_t capacity = {};

    if (unlikely(errno = ide_atapi_scsi_command(device, &packet, &capacity, sizeof(capacity))))
    {
        return errno;
    }

    *block_size = NATIVE_U32(capacity.block_size);
    *sectors    = NATIVE_U32(capacity.lba) + 1;

    device->sector_size = *block_size;
    device->sectors = *sectors;

    return 0;
}
