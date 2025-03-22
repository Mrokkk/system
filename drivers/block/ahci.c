#define log_fmt(fmt) "ahci: " fmt
#include "ahci.h"

#include <arch/pci.h>
#include <kernel/fs.h>
#include <kernel/irq.h>
#include <kernel/wait.h>
#include <kernel/devfs.h>
#include <kernel/blkdev.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/execute.h>
#include <kernel/process.h>
#include <kernel/page_mmio.h>
#include <kernel/page_alloc.h>

#include "ata.h"
#include "sata.h"

#define DEBUG_AHCI      0

module_init(ahci_init);
module_exit(ahci_deinit);
KERNEL_MODULE(ahci);

static int ahci_blk_read(void* blkdev, size_t offset, void* buffer, size_t size, bool irq);

READONLY static blkdev_ops_t bops = {
    .read = &ahci_blk_read,
};

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
        case AHCI_CAP_ISS_3GBPS_RAW_MASK:   return "3 Gbps";
        case AHCI_CAP_ISS_6GBPS_RAW_MASK:   return "6 Gbps";
        default:                            return "unknown";
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

static int ahci_port_error_check(ahci_t* ahci, ahci_port_t* port)
{
    if (unlikely(port->regs->is & AHCI_PxIS_TFES))
    {
        log_warning("port %u: task file error", port->id);
        ahci_dump(ahci->hba);
        return -EIO;
    }
    else if (unlikely(port->regs->serr))
    {
        log_warning("port %u: error");
        ahci_dump(ahci->hba);
        return -EIO;
    }

    return 0;
}

static uintptr_t ahci_port_phys_addr(ahci_port_t* port, void* ptr)
{
    return addr(ptr) - addr(port->data) + page_phys(port->data_pages);
}

static void ahci_irq_handle(uint32_t, void* private, pt_regs_t*)
{
    scoped_irq_lock();

    ahci_t* ahci = private;

    uint32_t is = ahci->hba->is;

    for (int i = 0; is && i < AHCI_PORT_COUNT; ++i, is >>= 1)
    {
        ahci_port_t* port = ahci->ports[i];

        if (!(is & 1))
        {
            continue;
        }

        uint32_t port_is = port->regs->is;

        if (!port_is)
        {
            continue;
        }

        request_t* req;
        int errno = ahci_port_error_check(ahci, port);

        list_for_each_entry_safe(req, &port->requests, list_entry)
        {
            if (port->regs->ci & (1 << req->slot))
            {
                continue;
            }

            req->errno = errno;

            list_del(&req->list_entry);

            process_t* proc = wait_queue_pop(&req->queue);

            if (unlikely(!proc))
            {
                log_error("no process in the wait queue for request on port %u slot %u",
                    port->id,
                    req->slot);
                continue;
            }

            process_wake(proc);
        }

        port->regs->is = port_is;
    }

    ahci->hba->is = is;
}

static int ahci_irq_enable(ahci_t* ahci)
{
    pci_msi_cap_t msi_cap = {};

    int cap_ptr = pci_cap_find(ahci->pci, PCI_CAP_ID_MSI, &msi_cap, sizeof(msi_cap));

    if (errno_get(cap_ptr))
    {
        log_info("no MSI support");
        return 0;
    }

    log_notice("MSI supported; 64bit: %B, maskable: %B",
        !!(msi_cap.msg_ctrl & MSI_MSG_CTRL_64BIT),
        !!(msi_cap.msg_ctrl & MSI_MSG_CTRL_MASKABLE));

    int irq, errno;

    if (unlikely(errno = irq_allocate(&ahci_irq_handle, "ahci", 0, ahci, &irq)))
    {
        return errno;
    }

    ahci->interrupts = true;

    msi_cap.msg_ctrl |= MSI_MSG_CTRL_ENABLE;

    if (msi_cap.msg_ctrl & MSI_MSG_CTRL_64BIT)
    {
        msi_cap.b64.msg_addr = 0xfee00000;
        msi_cap.b64.msg_data = irq;
        msi_cap.b64.mask = 0;

        pci_config_write(ahci->pci, cap_ptr + 0x4, &msi_cap.b64.msg_addr, 8);
        pci_config_write(ahci->pci, cap_ptr + 0xc, &msi_cap.b64.msg_data, 4);

        if (msi_cap.msg_ctrl & MSI_MSG_CTRL_MASKABLE)
        {
            pci_config_write(ahci->pci, cap_ptr + 0x10, &msi_cap.b64.mask, 4);
        }
    }
    else
    {
        msi_cap.b32.msg_addr = 0xfee00000;
        msi_cap.b32.msg_data = irq;
        msi_cap.b32.mask = 0;

        pci_config_write(ahci->pci, cap_ptr + 0x4, &msi_cap.b32.msg_addr, 4);
        pci_config_write(ahci->pci, cap_ptr + 0x8, &msi_cap.b32.msg_data, 4);

        if (msi_cap.msg_ctrl & MSI_MSG_CTRL_MASKABLE)
        {
            pci_config_write(ahci->pci, cap_ptr + 0xc, &msi_cap.b32.mask, 4);
        }
    }

    pci_config_write(ahci->pci, cap_ptr, &msi_cap, 4);

    ahci->hba->ghc.ie = 1;

    if (unlikely(ahci->hba->ghc.ie != 1))
    {
        return -EIO;
    }

    return 0;
}

static void ahci_port_error_clear(ahci_port_t* port)
{
    port->regs->serr = 0xffffffff & (~(0x1f << 27) | ~(0xf << 12) | ~(0x3f << 2));
}

static int ahci_drive_wait(ahci_t* ahci, ahci_port_t* port)
{
    size_t time_elapsed = 0;
    while (port->regs->tfd & (ATA_SR_BSY | ATA_SR_DRQ))
    {
        if (unlikely(time_elapsed > 2500))
        {
            log_warning("port %u: timeout while waiting for drive to go idle", port->id);
            ahci_dump(ahci->hba);
            return -ETIMEDOUT;
        }
        udelay(100);
        ++time_elapsed;
    }
    return 0;
}

static int ahci_drive_await_transfer_finish(ahci_t* ahci, ahci_port_t* port, int slot)
{
    int errno;
    size_t time_elapsed = 0;

    while (1)
    {
        if (unlikely(time_elapsed > 1000))
        {
            log_warning("port %u: timeout", port->id);
            ahci_dump(ahci->hba);
            return -ETIMEDOUT;
        }
        else if (unlikely(errno = ahci_port_error_check(ahci, port)))
        {
            return errno;
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

static int ahci_port_check(ahci_t* ahci, int nr)
{
    uint32_t ssts = ahci->hba->ports[nr].ssts;
    uint32_t ipm = ssts & AHCI_PxSSTS_IPM;
    uint32_t det = ssts & AHCI_PxSSTS_DET;

    if (det != AHCI_PxSSTS_DET_PRESENT || ipm != AHCI_PxSSTS_IPM_ACTIVE)
    {
        log_info("port %u: DET=%#x, IPM=%#x", nr, det, ipm);
        return -ENODEV;
    }

    return 0;
}

static int ahci_port_cmd_slot_find(ahci_hba_port_t* port)
{
    uint32_t slots = port->sact | port->ci;
    for (int i = 0; i < AHCI_PORT_COUNT; i++)
    {
        if (!(slots & 1))
        {
            return i;
        }
        slots >>= 1;
    }
    return -1;
}

static int ahci_read(ata_device_t* device, uint32_t offset, uint32_t count, char* buffer, bool irq)
{
    int errno;
    ahci_t* ahci = device->data;
    ahci_port_t* port = ahci->ports[device->id];
    int slot = ahci_port_cmd_slot_find(port->regs);
    uint32_t sectors = count / device->sector_size;

    if (unlikely(slot == -1))
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
    prdt->dba       = paddr;
    prdt->dbau      = 0;
    prdt->dbc       = count - 1;
    prdt->i         = 1;

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

    if (unlikely(errno = ahci_drive_wait(ahci, port)))
    {
        return errno;
    }

    if (ahci->interrupts && irq)
    {
        flags_t flags;

        irq_save(flags);
        request_t req = {.slot = slot};

        wait_queue_head_init(&req.queue);
        list_init(&req.list_entry);

        WAIT_QUEUE_DECLARE(q, process_current);

        list_add_tail(&req.list_entry, &port->requests);

        port->regs->ci = 1 << slot;

        if (unlikely(errno = process_wait_locked(&req.queue, &q, &flags)))
        {
            irq_restore(flags);
            return errno;
        }

        if (unlikely(errno = req.errno))
        {
            irq_restore(flags);
            return errno;
        }

        irq_restore(flags);
    }
    else
    {
        port->regs->ci = 1 << slot;
        if ((errno = ahci_drive_await_transfer_finish(ahci, port, slot)))
        {
            return errno;
        }
    }

    return 0;
}

static int ahci_blk_read(void* blkdev, size_t offset, void* buffer, size_t size, bool irq)
{
    int errno;
    ata_device_t* device = blkdev;
    size_t count = size * device->sector_size;

    if (unlikely(errno = ahci_read(device, offset, count, buffer, irq)))
    {
        log_warning("read error!");
        return errno;
    }

    return 0;
}

static int ahci_mode_set(ahci_t* ahci)
{
    ahci->hba->ghc.ae = 1;
    return ahci->hba->ghc.ae != 1;
}

static int ahci_ownership_take(ahci_t* ahci)
{
    ahci_hba_t* hba = ahci->hba;

    if (!(hba->cap2 & AHCI_CAP2_BOH))
    {
        return 0;
    }

    int res;
    uint32_t bohc = hba->bohc;

    log_info("OS ownership: %u", bohc & AHCI_BOHC_OOS);

    hba->bohc |= AHCI_BOHC_OOS;
    res = ahci_wait(&hba->bohc, AHCI_BOHC_BOS, AHCI_BOHC_BOS);
    res |= ahci_wait(&hba->bohc, AHCI_BOHC_BB, AHCI_BOHC_BB);

    return res | !(hba->bohc & AHCI_BOHC_OOS);
}

static int ahci_reset(ahci_t* ahci)
{
    if (ahci->hba->ghc.hr)
    {
        return 0;
    }

    ahci->hba->ghc.hr = 1;

    if (ahci_wait(&ahci->hba->ghc.value, AHCI_GHC_HR, AHCI_GHC_HR))
    {
        return 1;
    }

    return 0;
}

static void ahci_controller_print(ahci_t* ahci)
{
    uint32_t cap = ahci->hba->cap.value;

    log_info("AHCI mode: %B", ahci->hba->ghc.ae);
    log_continue("; speed: %s (%#x); 64-bit: %B", ahci_iss_string(ahci->hba->cap.iss), ahci->hba->cap.iss, ahci->hba->cap.s64a);
    log_continue("; ver: %X.%X", ahci->hba->vs.mjr, ahci->hba->vs.mnr);
    log_continue("; cmd slots: %u", ((cap & AHCI_CAP_NCS) >> AHCI_CAP_NCS_BIT) + 1);
    log_continue("; pi: %#x", ahci->hba->pi);
}

static int ahci_port_setup(ahci_t* ahci, size_t id)
{
    if (unlikely(id >= AHCI_PORT_COUNT))
    {
        log_warning("unsupported port number: %u", id);
        return -EINVAL;
    }

    ahci_port_t* port = zalloc(ahci_port_t);

    if (unlikely(!port))
    {
        return -ENOMEM;
    }

    port->data_pages = page_alloc(1, PAGE_ALLOC_UNCACHED | PAGE_ALLOC_ZEROED);

    list_init(&port->requests);

    if (unlikely(!port->data_pages))
    {
        log_warning("cannot allocate page for port %u", id);
        return -ENOMEM;
    }

    port->id   = id;
    port->data = page_virt_ptr(port->data_pages);
    port->regs = &ahci->hba->ports[id];

    port->regs->cmd &= ~AHCI_PxCMD_ST;
    while (port->regs->cmd & AHCI_PxCMD_CR);

    port->regs->cmd &= ~AHCI_PxCMD_FRE;
    while (port->regs->cmd & (AHCI_PxCMD_FRE | AHCI_PxCMD_CR));

    port->regs->clb  = ahci_port_phys_addr(port, &port->data->cmdlist);
    port->regs->clbu = 0;
    port->regs->fb   = ahci_port_phys_addr(port, &port->data->fis);
    port->regs->fbu  = 0;

    log_debug(DEBUG_AHCI, "clb = %#x, fb = %#x", port->regs->clb, port->regs->fb);

    for (int i = 0; i < 1; ++i)
    {
        ahci_command_t* cmdheader = &port->data->cmdlist[i];
        cmdheader->ctba = ahci_port_phys_addr(port, &port->data->cmdtable);
        log_debug(DEBUG_AHCI, "ctba = %#x", cmdheader->ctba);
    }

    if (ahci->hba->cap.sss)
    {
        port->regs->cmd |= AHCI_PxCMD_SUD;
        mdelay(20);
        while (port->regs->cmd & AHCI_PxCMD_CR);
    }

    if (ahci_port_check(ahci, id))
    {
        log_continue(": no device");
        return -ENODEV;
    }

    port->regs->cmd |= AHCI_PxCMD_FRE;
    while (port->regs->cmd & AHCI_PxCMD_CR);

    ahci_port_error_clear(port);

    port->regs->cmd |= AHCI_PxCMD_ST;

    uint32_t ssts = ahci->hba->ports[id].ssts;
    uint32_t ipm = ssts & AHCI_PxSSTS_IPM;
    uint32_t det = ssts & AHCI_PxSSTS_DET;
    uint32_t sig = ahci->hba->ports[id].sig;

    log_info("port %u: DET=%#x, IPM=%#x; SIG=%#x (%s)", id, det, ipm, sig, ahci_signature_string(sig));

    port->signature = sig;

    ahci->ports[id] = port;

    return 0;
}

static void ahci_port_detect(ahci_t* ahci, ahci_port_t* port)
{
    int errno;
    int id = port->id;
    int slot = ahci_port_cmd_slot_find(port->regs);
    FIS_REG_H2D* fis = ptr(&port->data->cmdtable.cfis);

    page_t* page = page_alloc(1, PAGE_ALLOC_ZEROED);

    if (unlikely(!page))
    {
        log_warning("cannot allocate buffer");
        return;
    }

    uint8_t* buf = page_virt_ptr(page);

    if (unlikely(slot == -1))
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
    prdt->dbc       = ATA_IDENT_SIZE - 1;

    fis->fis_type   = FIS_TYPE_REG_H2D;
    fis->command    = port->signature == AHCI_PxSIG_ATAPI ? ATA_CMD_IDENTIFY_PACKET : ATA_CMD_IDENTIFY;
    fis->device     = 0;    // Master device
    fis->c          = 1;    // Write command register

    if (unlikely(errno = ahci_drive_wait(ahci, port)))
    {
        return;
    }

    port->regs->ci = 1 << slot;

    if (!ahci_drive_await_transfer_finish(ahci, port, slot))
    {
        ata_device_t* device = alloc(ata_device_t);

        if (unlikely(!device))
        {
            log_warning("[port %u] no memory for ata_device_t", id);
            return;
        }

        ata_device_initialize(device, buf, id, ahci);

        ahci->devices[id] = device;
    }

    port->regs->ie = 0xffffffff;

    pages_free(page);
}

static int ahci_device_register(ata_device_t* device)
{
    int errno;

    blkdev_char_t blk = {
        .major = device->type == ATA_TYPE_ATAPI ? MAJOR_BLK_CDROM : MAJOR_BLK_SATA,
        .model = device->model,
        .sectors = device->sectors,
        .block_size = device->sector_size,
    };

    if (unlikely(errno = blkdev_register(&blk, device, &bops)))
    {
        log_info("failed to register blk device");
        return errno;
    }

    return 0;
}

static int ahci_ports_initialize(ahci_t* ahci)
{
    uint32_t pi = ahci->hba->pi;

    for (size_t i = 0; i < AHCI_PORT_COUNT; ++i, pi >>= 1)
    {
        if (!(pi & 1))
        {
            log_debug(DEBUG_AHCI, "port %u: not implemented", i);
            continue;
        }

        if (!ahci_port_setup(ahci, i))
        {
            ahci_port_detect(ahci, ahci->ports[i]);
        }
    }

    for (int i = 0; i < AHCI_PORT_COUNT; ++i)
    {
        if (!ahci->devices[i] || !ahci->devices[i]->signature)
        {
            continue;
        }
        ahci_device_register(ahci->devices[i]);
    }

    return 0;
}

UNMAP_AFTER_INIT void ahci_init_one(pci_device_t* pci_device, void*)
{
    ahci_hba_t* hba;

    if (pci_device->class != PCI_STORAGE || pci_device->subclass != PCI_STORAGE_SATA)
    {
        return;
    }

    bar_t* abar = &pci_device->bar[5];

    if (unlikely(!abar->addr || !abar->size || abar->region != PCI_MEMORY))
    {
        return;
    }

    if (unlikely(!(hba = mmio_map_uc(pci_device->bar[5].addr, pci_device->bar[5].size, "ahci"))))
    {
        log_warning("cannot map HBA");
        return;
    }

    ahci_t* ahci = zalloc(ahci_t);

    if (unlikely(!ahci))
    {
        mmio_unmap(hba);
        return;
    }

    ahci->hba = hba;
    ahci->pci = pci_device;

    if (execute_no_ret(pci_device_print(pci_device)) ||
        execute(pci_device_initialize(pci_device), "initialize PCI") ||
        execute(ahci_mode_set(ahci),               "set AHCI mode") ||
        execute(ahci_ownership_take(ahci),         "take ownership") ||
        execute(ahci_reset(ahci),                  "perform reset") ||
        execute(ahci_mode_set(ahci),               "set AHCI mode after reset") ||
        execute(ahci_irq_enable(ahci),             "enable IRQ") ||
        execute_no_ret(ahci_controller_print(ahci)) ||
        execute(ahci_ports_initialize(ahci),       "initialize ports"))
    {
        log_warning("cannot initialize AHCI controller");
        ahci_dump(ahci->hba);
        mmio_unmap(hba);
        delete(ahci);
    }
}

UNMAP_AFTER_INIT int ahci_init(void)
{
    pci_device_enumerate(&ahci_init_one, NULL);
    return 0;
}

int ahci_deinit(void)
{
    return 0;
}
