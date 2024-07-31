#define log_fmt(fmt) "ahci: " fmt
#include "ahci.h"

#include <arch/pci.h>
#include <kernel/fs.h>
#include <kernel/irq.h>
#include <kernel/page.h>
#include <kernel/devfs.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/process.h>

#include "ata.h"
#include "sata.h"

#define DEBUG_AHCI      0
#define AHCI_RESET      1

module_init(ahci_init);
module_exit(ahci_deinit);
KERNEL_MODULE(ahci);

static int ahci_fs_open(file_t* file);
static int ahci_fs_read(file_t* file, char* buf, size_t count);

static file_operations_t ops = {
    .open = &ahci_fs_open,
    .read = &ahci_fs_read,
};

static ahci_hba_t* ahci;
static ahci_port_t ports[AHCI_PORT_COUNT];
static pci_device_t* ahci_pci;
ata_device_t devices[AHCI_PORT_COUNT];

static_assert(sizeof(ahci_hba_t) == 0x1100);
static_assert(sizeof(ahci_hba_port_t) == 0x80);
static_assert(sizeof(ahci_received_fis_t) == 0x100);
static_assert(sizeof(ahci_command_t) == 32);
static_assert(sizeof(FIS_REG_H2D) == 20);
static_assert(sizeof(ahci_command_table_t) == 128);
static_assert(sizeof(ahci_prdt_entry_t) == 16);
static_assert(sizeof(ahci_port_data_t) <= PAGE_SIZE);
static_assert(offsetof(ahci_port_data_t, fis) == 0);
static_assert(offsetof(ahci_port_data_t, cmdlist) == 0x400);
static_assert(offsetof(ahci_port_data_t, cmdtable) == 0x800);
static_assert(offsetof(ahci_port_data_t, prdt) == 0x880);

// Memory model used by AHCI driver
//
// ABAR (ahci); offset from PCI.BAR[5]:
// [0x0000 - 0x002b] Generic Host Control
// [0x002c - 0x009f] Reserved
// [0x00a0 - 0x00ff] Vendor Specific
// [0x0100 - 0x1100] Ports
//   clb = Physical Address of Port N Command List
//   fb = Physical Address of Port N FIS
//
// data:
// [0x0000 - 0x01ff] Port 0 FIS (ahci_received_fis_t)
// [0x0400 - 0x04ff] Port 0 Command List (ahci_command_t); must be 1K aligned
//   ctba = Physical Address of Port 0 Command Table[n]
// [0x0800 - 0x08bf] Port 0 Command Table (ahci_command_table_t + ahci_prdt_entry_t * 4)

#define execute(operation, fail_string) \
    ({ int ret = operation; if (ret) { log_warning("%s: failed with %d", fail_string, ret);  }; ret; })

#define execute_no_ret(operation) \
    ({ operation; 0; })

static const char* ahci_signature_string(uint32_t sig)
{
    switch (sig)
    {
        case AHCI_PxSIG_ATA: return "SATA";
        case AHCI_PxSIG_ATAPI: return "ATAPI";
        case AHCI_PxSIG_SEMB: return "SEMB";
        case AHCI_PxSIG_PM: return "PM";
        default: return "no device";
    }
}

static const char* ahci_iss_string(uint32_t iss)
{
    switch (iss)
    {
        case AHCI_CAP_ISS_1_5GBPS_RAW_MASK: return "1.5 Gbps";
        case AHCI_CAP_ISS_3GBPS_RAW_MASK: return "3 Gbps";
        case AHCI_CAP_ISS_6GBPS_RAW_MASK: return "6 Gbps";
        default: return "unknown";
    }
}

static int ahci_wait(io32* reg, uint32_t mask, uint32_t val)
{
    int timeout = 10000;
    while ((*reg & mask) == val && --timeout)
    {
        io_delay(10);
    }
    return timeout == 0;
}

static uint32_t ahci_port_phys_addr(ahci_port_t* port, void* ptr)
{
    return addr(ptr) - addr(port->data) + page_phys(port->data_pages);
}

static int ahci_irq_enable(void)
{
    ahci->ghc.ie = 1;
    return ahci->ghc.ie != 1;
}

static void ahci_port_error_clear(ahci_port_t* port)
{
    port->regs->serr = 0xffffffff & (~(0x1f << 27) | ~(0xf << 12) | ~(0x3f << 2) );
}

static int ahci_drive_wait(ahci_port_t* port)
{
    size_t time_elapsed = 0;
    while (port->regs->tfd & (ATA_SR_BSY | ATA_SR_DRQ))
    {
        if (unlikely(time_elapsed > 250))
        {
            log_warning("port %u: timeout while waiting for drive to go idle", port->id);
            ahci_dump(ahci);
            return -ETIMEDOUT;
        }
        udelay(1000);
        ++time_elapsed;
    }
    return 0;
}

static int ahci_drive_await_transfer_finish(ahci_port_t* port, int slot)
{
    size_t time_elapsed = 0;

    while (1)
    {
        if (unlikely(time_elapsed > 1000))
        {
            log_warning("port %u: timeout", port->id);
            ahci_dump(ahci);
            return -ETIMEDOUT;
        }
        else if (unlikely(port->regs->is & AHCI_PxIS_TFES))
        {
            log_warning("port %u: task file error", port->id);
            ahci_dump(ahci);
            return -EIO;
        }
        else if (unlikely(port->regs->serr))
        {
            log_warning("port %u: error");
            ahci_dump(ahci);
            return -EIO;
        }
        // In some longer duration reads, it may be helpful to spin on the DPS bit
        // in the PxIS port field as well (1 << 5)
        if (!(port->regs->ci & (1 << slot)))
        {
            break;
        }
        udelay(1000);
        ++time_elapsed;
    }

    return port->regs->is & AHCI_PxIS_TFES
        ? -EIO
        : 0;
}

static int ahci_port_check(int nr)
{
    uint32_t ssts = ahci->ports[nr].ssts;
    uint32_t ipm = ssts & AHCI_PxSSTS_IPM;
    uint32_t det = ssts & AHCI_PxSSTS_DET;

    if (det != AHCI_PxSSTS_DET_PRESENT || ipm != AHCI_PxSSTS_IPM_ACTIVE)
    {
        log_info("port %u: DET=%x, IPM=%x", nr, det, ipm);
        return AHCI_DEV_NULL;
    }

    return 1;
}

static int ahci_port_cmd_slot_find(ahci_hba_port_t *port)
{
    uint32_t slots = port->sact | port->ci;
    for (int i = 0; i < 32; i++)
    {
        if (!(slots & 1))
        {
            return i;
        }
        slots >>= 1;
    }
    return -1;
}

static int ahci_read(ata_device_t* device, uint32_t offset, uint32_t count, char* buffer)
{
    int errno;
    ahci_port_t* port = ports + device->id;
    int slot = ahci_port_cmd_slot_find(port->regs);
    uint32_t sectors = count / ATA_SECTOR_SIZE;

    if (slot == -1)
    {
        return -EBUSY;
    }

    uint32_t paddr = vm_paddr(addr(buffer), process_current->mm->pgd);

    if (unlikely(!paddr))
    {
        return -EFAULT;
    }

    ahci_port_error_clear(port);

    log_debug(DEBUG_AHCI, "device: %u, offset: %u, count: %u, slot: %u", device->id, offset, count, slot);

    port->data->cmdlist[slot].cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    port->data->cmdlist[slot].w = 0;
    port->data->cmdlist[slot].prdtl = 1;

    ahci_prdt_entry_t* prdt = &port->data->prdt[0];
    prdt->dba = paddr;
    prdt->dbau = 0;
    prdt->dbc = count - 1;
    prdt->i = 1;

    FIS_REG_H2D* fis = ptr(&port->data->cmdtable.cfis);
    fis->fis_type   = FIS_TYPE_REG_H2D;
    fis->command    = ATA_CMD_READ_DMA_EXT;
    fis->c          = 1;

    fis->lba0 = (uint8_t)offset;
    fis->lba1 = (uint8_t)(offset >> 8);
    fis->lba2 = (uint8_t)(offset >> 16);
    fis->device = 1 << 6; // LBA mode
    fis->lba3 = (uint8_t)(offset >> 24);
    fis->lba4 = 0;
    fis->lba5 = 0;
    fis->countl = sectors & 0xff;
    fis->counth = (sectors >> 8) & 0xff;

    if ((errno = ahci_drive_wait(port)))
    {
        return errno;
    }

    port->regs->ci = 1 << slot;

    if ((errno = ahci_drive_await_transfer_finish(port, slot)))
    {
        return errno;
    }

    return 0;
}

static void ahci_irq_handle()
{
    log_debug(DEBUG_AHCI, "");
}

static int ahci_fs_open(file_t* file)
{
    int drive = BLK_DRIVE(MINOR(file->inode->rdev));
    if (!devices[drive].mbr.signature)
    {
        return -ENODEV;
    }
    return 0;
}

static int ahci_fs_read(file_t* file, char* buf, size_t count)
{
    int errno;
    int drive = BLK_DRIVE(MINOR(file->inode->rdev));
    ata_device_t* device = devices + drive;
    int partition = BLK_PARTITION(MINOR(file->inode->rdev));
    uint32_t offset = file->offset / ATA_SECTOR_SIZE;
    uint32_t first_sector, last_sector, max_count;

    if (partition != BLK_NO_PARTITION)
    {
        first_sector = device->mbr.partitions[partition].lba_start;
        offset += first_sector;
        last_sector = first_sector + device->mbr.partitions[partition].sectors;
    }
    else
    {
        last_sector = devices->size;
    }

    max_count = (last_sector - offset) * ATA_SECTOR_SIZE;
    count = count > max_count
        ? max_count
        : count;

    if (!count)
    {
        return 0;
    }

    if ((errno = ahci_read(device, offset, count, buf)))
    {
        log_warning("read error!");
        return errno;
    }

    file->offset += count;

    return count;
}

static int ahci_mode_set(void)
{
    ahci->ghc.ae = 1;
    return ahci->ghc.ae != 1;
}

static int ahci_ownership_take(void)
{
    if (!(ahci->cap2 & AHCI_CAP2_BOH))
    {
        return 0;
    }

    int res;
    uint32_t bohc = ahci->bohc;

    log_info("OS ownership: %u", bohc & AHCI_BOHC_OOS);

    ahci->bohc |= AHCI_BOHC_OOS;
    res = ahci_wait(&ahci->bohc, AHCI_BOHC_BOS, AHCI_BOHC_BOS);
    res |= ahci_wait(&ahci->bohc, AHCI_BOHC_BB, AHCI_BOHC_BB);

    return res | !(ahci->bohc & AHCI_BOHC_OOS);
}

static int ahci_reset(void)
{
    if (ahci->ghc.hr)
    {
        return 0;
    }

    ahci->ghc.hr = 1;

    if (ahci_wait(&ahci->ghc.value, AHCI_GHC_HR, AHCI_GHC_HR))
    {
        return 1;
    }

    return 0;
}

static void ahci_controller_print(void)
{
    uint32_t cap = ahci->cap.value;

    log_info("AHCI mode: %B", ahci->ghc.ae);
    log_continue("; speed: %s (%x); 64-bit: %B", ahci_iss_string(ahci->cap.iss), ahci->cap.iss, ahci->cap.s64a);
    log_continue("; ver: %X.%X", ahci->vs.mjr, ahci->vs.mnr);
    log_continue("; cmd slots: %u", ((cap & AHCI_CAP_NCS) >> AHCI_CAP_NCS_BIT) + 1);
    log_continue("; pi: %x", ahci->pi);
}

static int ahci_port_setup(int id)
{
    ahci_port_t* port = &ports[id];

    if (unlikely(id >= AHCI_PORT_COUNT))
    {
        log_warning("unsupported port number: %u", id);
        return -EINVAL;
    }

    port->data_pages = page_alloc(1, PAGE_ALLOC_DISCONT | PAGE_ALLOC_UNCACHED);

    if (unlikely(!port->data_pages))
    {
        log_warning("cannot allocate page for port %u", id);
        return -ENOMEM;
    }

    port->id    = id;
    port->data  = page_virt_ptr(port->data_pages);
    port->regs  = &ahci->ports[id];
    memset(port->data, 0, PAGE_SIZE);

    port->regs->cmd &= ~AHCI_PxCMD_ST;
    while (port->regs->cmd & AHCI_PxCMD_CR);

    port->regs->cmd &= ~AHCI_PxCMD_FRE;
    while (port->regs->cmd & (AHCI_PxCMD_FRE | AHCI_PxCMD_CR));

    port->regs->clbu    = 0;
    port->regs->clb     = ahci_port_phys_addr(port, &port->data->cmdlist);
    port->regs->fbu     = 0;
    port->regs->fb      = ahci_port_phys_addr(port, &port->data->fis);

    log_debug(DEBUG_AHCI, "clb = %x, fb = %x", port->regs->clb, port->regs->fb);

    for (int i = 0; i < 1; ++i)
    {
        ahci_command_t* cmdheader = &port->data->cmdlist[i];
        cmdheader->ctba = ahci_port_phys_addr(port, &port->data->cmdtable);
        log_debug(DEBUG_AHCI, "ctba = %x", cmdheader->ctba);
    }

    if (ahci->cap.sss)
    {
        port->regs->cmd |= AHCI_PxCMD_SUD;
        mdelay(20);
        while (port->regs->cmd & AHCI_PxCMD_CR);
    }

    if (ahci_port_check(id) == AHCI_DEV_NULL)
    {
        log_continue(": no device");
        return -ENODEV;
    }

    port->regs->cmd |= AHCI_PxCMD_FRE;
    while (port->regs->cmd & AHCI_PxCMD_CR);

    ahci_port_error_clear(port);

    port->regs->cmd |= AHCI_PxCMD_ST;

    uint32_t ssts = ahci->ports[id].ssts;
    uint32_t ipm = ssts & AHCI_PxSSTS_IPM;
    uint32_t det = ssts & AHCI_PxSSTS_DET;

    uint32_t sig = ahci->ports[id].sig;
    log_info("port %u: DET = %x, IPM = %x; SIG = %x (%s)", id, det, ipm, sig, ahci_signature_string(sig));

    port->signature = sig;

    //port->regs->ie |= 0xf;

    return 0;
}

static void ahci_port_detect(ahci_port_t* port)
{
    int errno;
    int id = port->id;
    int slot = ahci_port_cmd_slot_find(port->regs);
    FIS_REG_H2D* fis = ptr(&port->data->cmdtable.cfis);
    ata_device_t* device = devices + id;

    page_t* page = page_alloc1();

    if (unlikely(!page))
    {
        log_warning("cannot allocate buffer");
        return;
    }

    uint8_t* buf = page_virt_ptr(page);

    memset(buf, 0, PAGE_SIZE);

    if (slot == -1)
    {
        log_warning("cannot find empty slot!");
        return;
    }

    ahci_command_t* cmd = &port->data->cmdlist[slot];
    cmd->cfl        = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmd->w          = 0;
    cmd->prdtl      = 1;
    cmd->prdbc      = 0;
    cmd->p          = 1;

    ahci_prdt_entry_t* prdt = &port->data->prdt[0];
    prdt->dba       = page_phys(page);
    prdt->dbau      = 0;
    prdt->dbc       = 512 - 1;

    fis->fis_type   = FIS_TYPE_REG_H2D;
    fis->command    = port->signature == AHCI_PxSIG_ATAPI ? ATA_CMD_IDENTIFY_PACKET : ATA_CMD_IDENTIFY;
    fis->device     = 0;    // Master device
    fis->c          = 1;    // Write command register

    if ((errno = ahci_drive_wait(port)))
    {
        return;
    }

    port->regs->ci = 1 << slot;

    if (!ahci_drive_await_transfer_finish(port, slot))
    {
        ata_device_initialize(device, buf, id, NULL);
    }

    page_free(buf);
}

static int ahci_device_register(ata_device_t* device)
{
    int errno;
    partition_t* p;
    char* buf = single_page();
    mbr_t* mbr = ptr(buf);
    char drive_dev_name[8];

    ata_device_print(device);

    devfs_blk_register(
        fmtstr(drive_dev_name, "sd%c", device->id + 'a'),
        MAJOR_BLK_AHCI, BLK_MINOR_DRIVE(device->id),
        &ops);

    if (device->type == ATA_TYPE_ATAPI)
    {
        log_info("ATAPI not yet supported");
        return 0;
    }

    if (unlikely(!buf))
    {
        return -ENOMEM;
    }

    memset(buf, 0, PAGE_SIZE);

    if ((errno = ahci_read(device, 0, ATA_SECTOR_SIZE, buf)))
    {
        return errno;
    }

    log_info("  mbr: id = %x, signature = %x", mbr->id, mbr->signature);

    if (mbr->signature != MBR_SIGNATURE)
    {
        return -ENODEV;
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
            fmtstr(buf, "%s%u", drive_dev_name, part_id),
            MAJOR_BLK_AHCI,
            BLK_MINOR(part_id, device->id),
            &ops);
    }

    return 0;
}

static int ahci_ports_initialize(void)
{
    uint32_t pi = ahci->pi;

    for (int i = 0; i < AHCI_PORT_COUNT; ++i, pi >>= 1)
    {
        if (!(pi & 1))
        {
            log_debug(DEBUG_AHCI, "port %u: not implemented", i);
            continue;
        }

        if (!ahci_port_setup(i))
        {
            ahci_port_detect(ports + i);
        }
    }

    for (int i = 0; i < AHCI_PORT_COUNT; ++i)
    {
        if (!devices[i].signature)
        {
            continue;
        }
        ahci_device_register(&devices[i]);
    }

    return 0;
}

int ahci_init(void)
{
    if (!(ahci_pci = pci_device_get(PCI_STORAGE, PCI_STORAGE_SATA)))
    {
        log_info("no AHCI controller");
        return 0;
    }

    if (unlikely(!(ahci = region_map(ahci_pci->bar[5].addr, ahci_pci->bar[5].size, "ahci"))))
    {
        log_warning("cannot map AHCI's ABAR region");
        return -ENOMEM;
    }

    (void)ahci_reset;
    (void)ahci_irq_enable;
    (void)ahci_irq_handle;

    pci_device_print(ahci_pci);

    if (execute(pci_device_initialize(ahci_pci), "initialize PCI") ||
        execute(ahci_mode_set(),                 "set AHCI mode") ||
        execute(ahci_ownership_take(),           "take ownership") ||
#if AHCI_RESET
        execute(ahci_reset(),                    "perform reset") ||
        execute(ahci_mode_set(),                 "set AHCI mode after reset") ||
#endif
        execute_no_ret(ahci_controller_print()) ||
        execute(ahci_ports_initialize(),         "initialize ports"))
    {
        log_warning("cannot initialize AHCI controller");
        ahci_dump(ahci);
        return -EIO;
    }

    return 0;
}

int ahci_deinit(void)
{
    return 0;
}
