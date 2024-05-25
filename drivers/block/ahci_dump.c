#include "ahci.h"

#include <kernel/kernel.h>

#define STRING(reg, val) \
    [AHCI_##reg##_##val##_BIT] = #val

static const char* ghc_cap[32] = {
    STRING(CAP, SXS),
    STRING(CAP, EMS),
    STRING(CAP, CCCS),
    STRING(CAP, SSS),
    STRING(CAP, PSC),
    STRING(CAP, SSC),
    STRING(CAP, PMD),
    STRING(CAP, FBSS),
    STRING(CAP, SPM),
    STRING(CAP, SAM),
    STRING(CAP, SCLO),
    STRING(CAP, SAL),
    STRING(CAP, SALP),
    STRING(CAP, SSNTF),
    STRING(CAP, SMPS),
    STRING(CAP, SNCQ),
    STRING(CAP, S64A)
};

static const char* ghc_ghc[32] = {
    STRING(GHC, AE),
    STRING(GHC, MRSM),
    STRING(GHC, IE),
    STRING(GHC, HR)
};

static const char* px_is[32] = {
    STRING(PxIS, CPDS),
    STRING(PxIS, TFES),
    STRING(PxIS, HBFS),
    STRING(PxIS, HBDS),
    STRING(PxIS, IFS),
    STRING(PxIS, INFS),
    STRING(PxIS, OFS),
    STRING(PxIS, IPMS),
    STRING(PxIS, PRCS),
    STRING(PxIS, DMPS),
    STRING(PxIS, PCS),
    STRING(PxIS, DPS),
    STRING(PxIS, UFS),
    STRING(PxIS, SDBS),
    STRING(PxIS, DSS),
    STRING(PxIS, PSS),
    STRING(PxIS, DHRS),
};

static const char* px_cmd[32] = {
    STRING(PxCMD, ASP),
    STRING(PxCMD, ATAPI),
    STRING(PxCMD, ESP),
    STRING(PxCMD, CPD),
    STRING(PxCMD, MPSP),
    STRING(PxCMD, HPCP),
    STRING(PxCMD, PMA),
    STRING(PxCMD, CPS),
    STRING(PxCMD, CR),
    STRING(PxCMD, FR),
    STRING(PxCMD, MPSS),
    STRING(PxCMD, CCS),
    STRING(PxCMD, FRE),
    STRING(PxCMD, CLO),
    STRING(PxCMD, POD),
    STRING(PxCMD, SUD),
    STRING(PxCMD, ST),
};

static const char* px_tfd[32] = {
    STRING(PxTFD, STS_BSY),
    STRING(PxTFD, STS_DRQ),
    STRING(PxTFD, STS_ERR),
};

#define ERROR(v, string) \
    [AHCI_PxSERR_##v##_BIT] = string

static const char* px_serr[32] = {
    ERROR(DIAG_X, "Exchanged"),
    ERROR(DIAG_F, "Unknown FIS Type"),
    ERROR(DIAG_T, "Transport state transition error"),
    ERROR(DIAG_S, "Link Sequence Error"),
    ERROR(DIAG_H, "Handshake Error"),
    ERROR(DIAG_C, "CRC Error"),
    ERROR(DIAG_D, "Disparity Error"),
    ERROR(DIAG_B, "10B to 8B Decode Error"),
    ERROR(DIAG_W, "Comm Wake"),
    ERROR(DIAG_I, "Phy Internal Error"),
    ERROR(DIAG_N, "PhyRdy Change"),
    ERROR(ERR_E, "Internal Error"),
    ERROR(ERR_P, "Protocol Error"),
    ERROR(ERR_C, "Persistent Communication or Data Integrity Error"),
    ERROR(ERR_T, "Transient Data Integrity Error"),
    ERROR(ERR_M, "Recovered Communications Error"),
    ERROR(ERR_I, "Recovered Data Integrity Error"),
};

static void bits_print(uint32_t value, const char** strings)
{
    for (int i = 0, j = 1; i < 32; ++i, j <<= 1)
    {
        if ((value & j) && strings[i])
        {
            log_continue("%s ", strings[i]);
        }
    }
}

static void ahci_ports_print(uint32_t value)
{
    for (int i = 0, j = 1; i < 32; ++i, j <<= 1)
    {
        if (value & j)
        {
            log_continue("P%u ", i);
        }
    }
}

void ahci_port_dump(ahci_hba_port_t* port)
{
    if (!(port->ssts & AHCI_PxSSTS_DET_PRESENT))
    {
        log_continue("not detected");
        log_info("    PxSSTS=%x;", port->ssts);
        return;
    }
    log_info("    PxCLB=%x; PxCLBU=%x", port->clb, port->clbu);
    log_info("    PxFB=%x; PxFBU=%x", port->fb, port->fbu);
    log_info("    PxIS=%x;", port->is);
    bits_print(port->is, px_is);
    log_info("    PxIE=%x;", port->ie);
    log_info("    PxCMD=%x; ", port->cmd);
    bits_print(port->cmd, px_cmd);
    log_info("    PxTFD=%x;", port->tfd);
    bits_print(port->tfd, px_tfd);
    log_info("    PxSIG=%x;", port->sig);
    log_info("    PxSSTS=%x; IPM=%x SPD=%x DET=%x",
        port->ssts,
        (port->ssts & AHCI_PxSSTS_IPM_MASK) >> AHCI_PxSSTS_IPM_BIT,
        (port->ssts & AHCI_PxSSTS_SPD_MASK) >> AHCI_PxSSTS_SPD_BIT,
        port->ssts & AHCI_PxSSTS_DET);
    log_info("    PxSCTL=%x;", port->sctl);
    log_info("    PxSERR=%x;", port->serr);
    bits_print(port->serr, px_serr);
    log_info("    PxSACT=%x;", port->sact);
    log_info("    PxCI=%x;", port->ci);
    log_info("    PxSNTF=%x;", port->sntf);
    log_info("    PxFBS=%x;", port->fbs);
}

void ahci_dump(ahci_hba_t* ahci)
{
    log_info("AHCI dump (%x):", ahci);
    log_info("  GHC.CAP=%x; ", ahci->cap);
    log_continue("NP=%u ", ahci->cap.np);
    log_continue("NCS=%u ", ahci->cap.ncs);
    log_continue("ISS=%x ", ahci->cap.iss);
    bits_print(ahci->cap.value, ghc_cap);
    log_info("  GHC.GHC=%x; ", ahci->ghc);
    bits_print(ahci->ghc.value, ghc_ghc);
    log_info("  GHC.IS=%x; ", ahci->is);
    ahci_ports_print(ahci->is);
    log_info("  GHC.PI=%x; ", ahci->pi);
    ahci_ports_print(ahci->pi);
    log_info("  GHC.VS=%x; %X.%X", ahci->vs.value, ahci->vs.mjr, ahci->vs.mnr);
    log_info("  GHC.CCC_CTL=%x;", ahci->ccc_ctl);
    log_info("  GHC.CCC_PORTS=%x; ", ahci->ccc_pts);
    ahci_ports_print(ahci->ccc_pts);
    log_info("  GHC.CCC_PORTS=%x; ", ahci->ccc_pts);
    log_info("  GHC.CAP2=%x; ", ahci->cap2);
    log_info("  GHC.BOHC=%x; ", ahci->bohc);

    for (int i = 0, j = 1; i < 32; ++i, j <<= 1)
    {
        if (!(ahci->pi & j))
        {
            continue;
        }
        log_info("  Port %u: ", i);
        ahci_port_dump(&ahci->ports[i]);
    }
}
