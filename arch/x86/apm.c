#define log_fmt(fmt) "apm: " fmt
#include <arch/apm.h>
#include <arch/asm.h>
#include <arch/dmi.h>
#include <arch/acpi.h>
#include <arch/bios.h>
#include <arch/reboot.h>
#include <arch/segment.h>
#include <arch/descriptor.h>

#include <kernel/init.h>
#include <kernel/kernel.h>
#include <kernel/minmax.h>
#include <kernel/byteorder.h>
#include <kernel/page_alloc.h>

#ifdef __i386__

// Reference: https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/apmv12.rtf

#define APM_MODE            APM_32BIT_PROTECTED

#define APM_REALMODE        0x01
#define APM_16BIT_PROTECTED 0x02
#define APM_32BIT_PROTECTED 0x03

#define APM_PMDIS           0x01
#define APM_REALEST         0x02
#define APM_NOIF            0x03
#define APM_16PEST          0x05
#define APM_16PUNSUP        0x06
#define APM_32PEST          0x07
#define APM_32PUNSUP        0x08
#define APM_NODEV           0x09
#define APM_ERANGE          0x0a
#define APM_CANNOT_ENTER    0x60
#define APM_UNAVL           0x86

#define APM_FLAGS_32BIT_IF  (1 << 1)

#define APM_INSTALLATION_CHECK(regs) \
    ({ \
        memset(&regs, 0, sizeof(regs)); \
        regs.ax = 0x5300; \
        regs.bx = 0x0000; \
        &regs; \
    })

#define APM_INTERFACE_CONNECT(regs, mode) \
    ({ \
        memset(&regs, 0, sizeof(regs)); \
        regs.ax = 0x5300 | mode; \
        regs.bx = 0x0000; \
        &regs; \
    })

#define APM_INTERFACE_DISCONNECT(regs) \
    ({ \
        memset(&regs, 0, sizeof(regs)); \
        regs.ax = 0x5304; \
        regs.bx = 0x0000; \
        &regs; \
    })

#define APM_SHUTDOWN(regs) \
    ({ \
        memset(&regs, 0, sizeof(regs)); \
        regs.ax = 0x5307; \
        regs.bx = 0x0001; \
        regs.cx = 0x0003; \
        &regs; \
    })

#define APM_DRIVER_VERSION(regs) \
    ({ \
        memset(&regs, 0, sizeof(regs)); \
        regs.ax = 0x530e; \
        regs.bx = 0x0000; \
        regs.cx = 0x0102; \
        &regs; \
    })

#define APM_PM_ENABLE(regs) \
    ({ \
        memset(&regs, 0, sizeof(regs)); \
        regs.ax = 0x5308; \
        regs.bx = 0x0001; \
        regs.cx = 0x0001; \
        &regs; \
    })

READONLY static int (*apm_call)(regs_t* regs, const char* name);

READONLY struct
{
    uint32_t off;
    uint16_t seg;
} apm_entry;

static inline const char* apm_error(uint8_t ec)
{
    switch (ec)
    {
        case APM_PMDIS:     return "Power management functionality disabled";
        case APM_REALEST:   return "Real mode interface connection already established";
        case APM_16PEST:    return "16-bit protected mode interface connection already established";
        case APM_16PUNSUP:  return "16-bit protected mode interface not supported";
        case APM_32PEST:    return "32-bit protected mode interface already established";
        case APM_32PUNSUP:  return "32-bit protected mode interface not supported";
        case APM_NODEV:     return "Unrecognized device ID";
        case APM_ERANGE:    return "Parameter value out of range";
        case APM_CANNOT_ENTER: return "Unable to enter requested state";
        case APM_UNAVL:     return "APM not present";
    }
    return "Unknown";
}

static inline int apm_32bit_call(regs_t* regs, const char* name)
{
    asm volatile(
        "push %%ebp;"
        "pushl %%ds;"
        "pushl %%es;"
        "pushl %%fs;"
        "pushl %%gs;"
        "pushf;"
        "pushl $0;"
        "popf;"
        "mov "ASM_VALUE(APM_DS)", %%edx;"
        "mov %%edx, %%ds;"
        "mov %%edx, %%es;"
        "mov %%edx, %%fs;"
        "mov %%edx, %%gs;"
        "lcall *%%cs:apm_entry;"
        "push %%eax;"
        "pushf;"
        "pop %%eax;"
        "mov %%eax, %0;"
        "pop %%eax;"
        "popf;"
        "popl %%gs;"
        "popl %%fs;"
        "popl %%es;"
        "popl %%ds;"
        "pop %%ebp;"
        : "=a" (regs->eax), "=b" (regs->ebx), "=c" (regs->ecx), "=d" (regs->edx),
          "=D" (regs->edi), "=S" (regs->esi), "=m" (regs->eflags)
        : "a" (regs->eax), "b" (regs->ebx), "c" (regs->ecx), "d" (regs->edx)
        : "memory", "cc");

    if (unlikely(regs->eflags & EFL_CF))
    {
        log_info("%s: %s (%#x)", name, apm_error(regs->ah), regs->ah);
        return -1;
    }

    return 0;
}

static int apm_bios_call(regs_t* regs, const char* name)
{
    bios_call(BIOS_SYSTEM, regs);

    if (unlikely(regs->eflags & EFL_CF))
    {
        log_notice("%s: %s (%#x)", name, apm_error(regs->ah), regs->ah);
        return -1;
    }

    return 0;
}

static void apm_shutdown(void)
{
    regs_t regs;
    log_info("performing shutdown");
    apm_call(APM_SHUTDOWN(regs), "shutdown");
}

static int apm_32bit_configure(regs_t* regs)
{
    uint32_t code_start, code_16, data_start;

    log_continue("; entry: %#06x:%#06x", regs->ax, regs->ebx);

    code_start = regs->ax * 16;
    data_start = regs->dx * 16;
    code_16    = regs->cx * 16;

    apm_entry.off = regs->ebx;
    apm_entry.seg = APM_CS;

    uint16_t cs32_limit = ((regs->esi & 0xffff) - 1) & 0xffff;
    uint16_t cs16_limit = ((regs->esi >> 16) - 1) & 0xffff;
    uint16_t ds_limit = (regs->di - 1) & 0xffff;

    descriptor_set_base(gdt_entries, APM_CODE_ENTRY, code_start);
    descriptor_set_base(gdt_entries, APM_CODE_16_ENTRY, code_16);
    descriptor_set_base(gdt_entries, APM_DATA_ENTRY, data_start);

    descriptor_set_limit(gdt_entries, APM_CODE_ENTRY, cs32_limit);
    descriptor_set_limit(gdt_entries, APM_CODE_16_ENTRY, cs16_limit);
    descriptor_set_limit(gdt_entries, APM_DATA_ENTRY, ds_limit);

    apm_call = &apm_32bit_call;

    return 0;
}

UNMAP_AFTER_INIT void apm_initialize(void)
{
    int apm_mode = APM_MODE;
    regs_t regs;

    static_assert(APM_MODE == APM_REALMODE || APM_MODE == APM_32BIT_PROTECTED);

    if (param_bool_get(KERNEL_PARAM("noapm")))
    {
        log_notice("disabled through kernel parameter");
        return;
    }

    if (apm_bios_call(APM_INSTALLATION_CHECK(regs), "installation check"))
    {
        return;
    }

    if (regs.bx != U16('M', 'P'))
    {
        log_notice("installation check: incorrect signature: %#x", regs.bx);
        return;
    }

    log_notice("APM %x.%x, flags: %#x", regs.ax >> 8, regs.ax & 0xff, regs.cx);

    if (regs.ax != 0x102 && regs.ax != 0x101 && regs.ax != 0x100)
    {
        log_continue("; unsupported version");
        return;
    }

    if (!(regs.cx & APM_FLAGS_32BIT_IF))
    {
        log_notice("32bit interface not available");
        apm_mode = APM_REALMODE;
    }
    else if (dmi.bios && dmi.bios_version &&
        !strcmp("Award Software International, Inc.", dmi.bios) &&
        (!strcmp("4.51 PG", dmi.bios_version) || !strcmp("6.00 PG", dmi.bios_version)))
    {
        log_notice("[Award] disabling 32bit interface");
        apm_mode = APM_REALMODE;
    }

disconnect:
    apm_bios_call(APM_INTERFACE_DISCONNECT(regs), "disconnect interface");

    if (acpi_enabled)
    {
        log_notice("ACPI enabled; disconnected the interface");
        return;
    }

    if (apm_bios_call(APM_INTERFACE_CONNECT(regs, apm_mode), "connect interface"))
    {
        return;
    }

    switch (apm_mode)
    {
        case APM_32BIT_PROTECTED:
            if (apm_32bit_configure(&regs))
            {
                apm_mode = APM_REALMODE;
                log_notice("reconnecting as real mode interface");
                goto disconnect;
            }
            break;
        default:
            apm_call = &apm_bios_call;
    }

    if (apm_call(APM_DRIVER_VERSION(regs), "setting driver version") ||
        apm_call(APM_PM_ENABLE(regs), "enable PM"))
    {
        return;
    }

    shutdown_fn = &apm_shutdown;
}
#else

void apm_initialize(void)
{
}

#endif
