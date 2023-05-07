#define log_fmt(fmt) "ahci: " fmt
#include "ahci.h"
#include <arch/pci.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/backtrace.h>

#define AHCI_DISABLED   1
#define AHCI_PHYS       0x6000
#define FIS_SIZE        0x100
#define CMDLIST_SIZE    0x400
#define CMDTABLE_SIZE   (0x80 + sizeof(HBA_PRDT_ENTRY) * 4)

module_init(ahci_init);
module_exit(ahci_deinit);
KERNEL_MODULE(ahci);

static int ahci_port_check(int id);
static int ahci_wait(uint32_t reg, uint32_t mask, uint32_t val);
static int ahci_controller_reset(void);
static int ahci_ownership_take(void);
static void ahci_mode_set(void);
static void ahci_port_setup(int id);

static uint8_t* ahci;
static uint8_t* data;

static ahci_port_t ports[1];

static inline const char* ahci_speed_string(uint32_t cap)
{
    switch (cap & AHCI_CAP_ISS)
    {
        case AHCI_CAP_ISS_1_5GBPS: return "1.5 Gbps";
        case AHCI_CAP_ISS_3GBPS: return "3 Gbps";
        case AHCI_CAP_ISS_6GBPS: return "6 Gbps";
        default: return "unknown";
    }
}

static inline uint32_t ahci_readl(uint32_t reg)
{
    return readl(ahci + reg);
}

static inline void ahci_writel(uint32_t data, uint32_t reg)
{
    writel(data, ahci + reg);
}

static inline uint32_t ahci_port_reg(uint32_t port, uint32_t reg)
{
    return 0x100 + port * 0x80 + reg;
}

int ahci_init(void)
{
    if (AHCI_DISABLED)
    {
        return 0;
    }

    uint32_t pi, cap, vs, ghc;
    pci_device_t* ahci_pci = pci_device_get(PCI_STORAGE, PCI_STORAGE_SATA);

    if (!ahci_pci)
    {
        return 0;
    }

    pci_device_print(ahci_pci);

    ahci = region_map(ahci_pci->bar[5].addr, ahci_pci->bar[5].size, "ahci");

    ahci_mode_set();

    cap = ahci_readl(AHCI_CAP);
    vs = ahci_readl(AHCI_VS);
    ghc = ahci_readl(AHCI_GHC);
    pi = ahci_readl(AHCI_PI);

    log_info("AHCI mode: %u", !!(ghc & AHCI_GHC_AE));
    log_continue("; speed: %s; 64-bit: %u", ahci_speed_string(cap), !!(cap & AHCI_CAP_S64A));
    log_continue("; ver: %u.%u", vs >> 16, vs & 0xffff);
    log_continue("; cmd slots: %u", ((cap & AHCI_CAP_NCS) >> AHCI_CAP_NCS_BIT) + 1);

    data = region_map(AHCI_PHYS, 32 * PAGE_SIZE, "ahci2");

    if (ahci_ownership_take())
    {
        log_warning("failed to take ownership");
        return -EIO;
    }

    if (ahci_controller_reset())
    {
        log_warning("failed to reset");
        return -EIO;
    }

    for (int i = 0; i < 32; ++i, pi >>= 1)
    {
        if (pi & 1)
        {
            if (ahci_port_check(i) == AHCI_DEV_NULL)
            {
                continue;
            }

            ahci_port_setup(i);
        }
    }

    return 0;
}

int ahci_deinit(void)
{
    return 0;
}

static int ahci_port_check(int nr)
{
    uint32_t ssts = ahci_readl(ahci_port_reg(nr, AHCI_PxSSTS));
    uint32_t sig = ahci_readl(ahci_port_reg(nr, AHCI_PxSIG));

    uint32_t ipm = ssts & AHCI_PxSSTS_IPM;
    uint32_t det = ssts & AHCI_PxSSTS_DET;

    if (det != AHCI_PxSSTS_DET_PRESENT || ipm != AHCI_PxSSTS_IPM_ACTIVE)
    {
        return AHCI_DEV_NULL;
    }

    log_info("port%u: ", nr);

    switch (sig)
    {
        case SATA_SIG_ATAPI:
            log_continue("ATAPI");
            return AHCI_DEV_SATAPI;
        case SATA_SIG_SEMB:
            log_continue("SEMB");
            return AHCI_DEV_SEMB;
        case SATA_SIG_PM:
            log_continue("PM");
            return AHCI_DEV_PM;
        default:
            log_continue("SATA");
            return AHCI_DEV_SATA;
    }
}

static int ahci_wait(uint32_t reg, uint32_t mask, uint32_t val)
{
    int timeout = 10000;
    while ((readl(ahci + reg) & mask) == val && --timeout)
    {
        io_delay(10);
    }
    return timeout == 0;
}

static int ahci_controller_reset(void)
{
    uint32_t tmp = ahci_readl(AHCI_GHC);

    if ((tmp & AHCI_GHC_HR) == 0)
    {
        log_info("performing reset...");
        tmp |= AHCI_GHC_HR;
        ahci_writel(tmp, AHCI_GHC);
        tmp = ahci_readl(AHCI_GHC);

        if (ahci_wait(AHCI_GHC, AHCI_GHC_HR, AHCI_GHC_HR))
        {
            return 1;
        }

        log_continue(" finished");
    }

    return 0;
}

static int ahci_ownership_take(void)
{
    int res;
    uint32_t bohc = readl(ahci + AHCI_BOHC);
    log_info("OS ownership: %u", bohc & AHCI_BOHC_OOS);

    res = ahci_wait(AHCI_BOHC, AHCI_BOHC_BOS, AHCI_BOHC_BOS);
    res |= ahci_wait(AHCI_BOHC, AHCI_BOHC_BB, AHCI_BOHC_BB);

    return res;
}

static void ahci_mode_set(void)
{
    uint32_t ghc = ahci_readl(AHCI_GHC);
    ahci_writel(ghc | AHCI_GHC_AE, AHCI_GHC);
    ahci_readl(AHCI_GHC);
}

static void ahci_port_setup(int id)
{
    uint32_t cmd;
    uint8_t* base = data + id * PAGE_SIZE;
    ahci_port_t* port = ports + id;

    static_assert(FIS_SIZE + CMDLIST_SIZE + CMDTABLE_SIZE < PAGE_SIZE);

    if (id > 1)
    {
        log_warning("port no too high: %u", id);
    }

    memset(base, 0, PAGE_SIZE);

    port->base_reg = ahci_port_reg(id, 0);
    port->fis = ptr(base);
    port->cmdlist = ptr(base + FIS_SIZE);
    port->cmdtable = ptr(data + FIS_SIZE + CMDLIST_SIZE);

    ahci_writel(AHCI_PHYS + addr(port->cmdtable) - addr(data), port->base_reg + AHCI_PxCLB);
    ahci_writel(0, port->base_reg + AHCI_PxCLBU);
    ahci_writel(AHCI_PHYS + addr(port->fis) - addr(data), port->base_reg + AHCI_PxFB);
    ahci_writel(0, port->base_reg + AHCI_PxFBU);

    cmd = ahci_readl(port->base_reg + AHCI_PxCMD);
    ahci_writel(cmd | AHCI_PxCMD_FRE, port->base_reg + AHCI_PxCMD);

    /*FIS_REG_H2D* fis = ptr(port->fis);*/
    /*HBA_CMD_HEADER* cmdhdr = ptr(port->cmdlist);*/
    /*HBA_CMD_TBL* cmdtbl = ptr(port->cmdtable);*/

    /*fis->fis_type = FIS_TYPE_REG_H2D;*/
    /*fis->command = ATA_CMD_IDENTIFY;    // 0xEC*/
    /*fis->device = 0;            // Master device*/
    /*fis->c = 1;                // Write command register*/
}
