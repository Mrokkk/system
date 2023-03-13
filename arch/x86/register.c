#include <kernel/kernel.h>
#include <kernel/string.h>
#include <arch/register.h>

char* eflags_lobits[] = {
    "cf", 0, "pf", 0, "af", 0, "zf", "sf", "tf", "if", "df", "of", 0, 0, "nt", 0, "rf", 0, 0, 0, 0, "id"
};

char* eflags_hibits[] = {
    "CF", 0, "PF", 0, "AF", 0, "ZF", "SF", "TF", "IF", "DF", "OF", 0, 0, "NT", 0, "RF", 0, 0, 0, 0, "ID"
};

char* cr0_lobits[] = {
    "pe", "mp", "em", "ts", "et", "ne", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "wp", "am", "nw", "cd", "pg"
};

char* cr0_hibits[] = {
    "PE", "MP", "EM", "TS", "ET", "NE", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "WP", "AM", "NW", "CD", "PG"
};

char* cr4_lobits[] = {
    "vme", "pvi", "tsd", "de", "pse", "pae", "mce", "pge", "pce", "osfxsr", "osxmmexcpt", 0, 0, "vmxe",
    "smxe", 0, 0, "pcide", "osxsave", 0, "smep", "smap"
};

char* cr4_hibits[] = {
    "VME", "PVI", "TSD", "DE", "PSE", "PAE", "MCE", "PGE", "PCE", "OSFXSR", "OSXMMEXCPT", 0, 0, "VMXE",
    "SMXE", 0, 0, "PCIDE", "OSXSAVE", 0, "SMEP", "SMAP"
};

int eflags_bits_string_get(uint32_t eflags, char* buffer)
{
    int len = 0;

    for (int i = 0; i < 22; i++)
    {
        if (eflags_lobits[i])
        {
            eflags & (1 << i)
                ? sprintf(buffer+len, "%s ", eflags_hibits[i])
                : sprintf(buffer+len, "%s ", eflags_lobits[i]);
            len += strlen(eflags_lobits[i]) + 1;
        }
    }

    return len;
}

int cr0_bits_string_get(uint32_t cr0, char* buffer)
{
    int len = 0;

    for (int i = 0; i < 21; i++)
    {
        if (cr0_lobits[i])
        {
            cr0 & (1 << i)
                ? sprintf(buffer+len, "%s ", cr0_hibits[i])
                : sprintf(buffer+len, "%s ", cr0_lobits[i]);
            len += strlen(cr0_lobits[i]) + 1;
        }
    }

    return len;
}

int cr4_bits_string_get(uint32_t cr4, char* buffer)
{
    int len = 0;

    for (int i = 0; i < 22; i++)
    {
        if (cr4_lobits[i])
        {
            cr4 & (1 << i)
                ? sprintf(buffer+len, "%s ", cr4_hibits[i])
                : sprintf(buffer+len, "%s ", cr4_lobits[i]);
            len += strlen(cr4_lobits[i]) + 1;
        }
    }

    return len;
}
