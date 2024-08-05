#include <arch/register.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <kernel/bitflag.h>

static const char* eflags_bits[] = {
    "cf", 0, "pf", 0, "af", 0, "zf", "sf", "tf", "if", "df", "of", 0, 0, "nt", 0, "rf", "vm", "ac", 0, 0, "id"
};

static const char* cr0_bits[] = {
    "pe", "mp", "em", "ts", "et", "ne", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "wp", "am", "nw", "cd", "pg"
};

static const char* cr4_bits[] = {
    "vme", "pvi", "tsd", "de", "pse", "pae", "mce", "pge", "pce", "osfxsr", "osxmmexcpt", 0, 0, "vmxe",
    "smxe", 0, 0, "pcide", "osxsave", 0, "smep", "smap"
};

const char* eflags_bits_string_get(uint32_t eflags, char* buffer, size_t size)
{
    bitflags_string(buffer, size, eflags_bits, eflags);
    return buffer;
}

const char* cr0_bits_string_get(uint32_t cr0, char* buffer, size_t size)
{
    bitflags_string(buffer, size, cr0_bits, cr0);
    return buffer;
}

const char* cr4_bits_string_get(uint32_t cr4, char* buffer, size_t size)
{
    bitflags_string(buffer, size, cr4_bits, cr4);
    return buffer;
}

void regs_print(loglevel_t severity, const pt_regs_t* regs, const char* header)
{
    char buffer[64];
    char header_buffer[32];
    *header_buffer = 0;

    if (header)
    {
        snprintf(header_buffer, sizeof(header_buffer), "%s: ", header);
    }

    log(severity, "%seax: %08x ebx: %08x ecx: %08x", header_buffer, regs->eax, regs->ebx, regs->ecx);
    log(severity, "%sedx: %08x esi: %08x edi: %08x", header_buffer, regs->edx, regs->esi, regs->edi);

    log(severity, "%sebp: %08x esp: %02x:%08x",
        header_buffer,
        regs->ebp,
        regs->cs == KERNEL_CS ? ss_get() : regs->ss,
        regs->esp);

    log(severity, "%seip: %02x:%08x",
        header_buffer,
        regs->cs,
        regs->eip);

    log(severity, "%sds: %02x; es: %02x; fs: %02x; gs: %02x",
        header_buffer,
        regs->ds,
        regs->es,
        regs->fs,
        regs->gs);

    log(severity, "%seflags: %08x: (iopl=%d %s)",
        header_buffer,
        regs->eflags,
        (regs->eflags >> 12) & 0x3,
        eflags_bits_string_get(regs->eflags, buffer, sizeof(buffer)));
}
