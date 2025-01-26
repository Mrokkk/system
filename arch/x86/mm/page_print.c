#include <kernel/kernel.h>
#include <kernel/page_table.h>

static const char* pgd_print(const pgd_t* pgd, char* output, size_t size)
{
    const pgd_t val = *pgd;
    const char* orig_output = output;
    const char* end = output + size;

    output = csnprintf(output, end, "%#lx, flags=(", val & ~PAGE_MASK);
    output = csnprintf(output, end, (val & PAGE_PRESENT)
        ? "present "
        : "non-present ");
    output = csnprintf(output, end, (val & PAGE_RW)
        ? "writable "
        : "non-writable ");
    if (val & PAGE_GLOBAL)
    {
        output = csnprintf(output, end, "global ");
    }
    output = csnprintf(output, end, (val & PAGE_USER)
        ? "user)"
        : "kernel)");

    return orig_output;
}

static const char* pud_print(const pud_t* pud, char* output, size_t size)
{
    const pud_t val = *pud;
    const char* orig_output = output;
    const char* end = output + size;

    output = csnprintf(output, end, "%#lx, flags=(", val & ~PAGE_MASK);
    output = csnprintf(output, end, (val & PAGE_PRESENT)
        ? "present "
        : "non-present ");
    output = csnprintf(output, end, (val & PAGE_RW)
        ? "writable "
        : "non-writable ");
    if (val & PAGE_GLOBAL)
    {
        output = csnprintf(output, end, "global ");
    }
    output = csnprintf(output, end, (val & PAGE_USER)
        ? "user)"
        : "kernel)");

    return orig_output;
}

static const char* pmd_print(const pmd_t* pmd, char* output, size_t size)
{
    const pmd_t val = *pmd;
    const char* orig_output = output;
    const char* end = output + size;

    output = csnprintf(output, end, "%#lx, flags=(", val & ~PAGE_MASK);
    output = csnprintf(output, end, (val & PAGE_PRESENT)
        ? "present "
        : "non-present ");
    output = csnprintf(output, end, (val & PAGE_RW)
        ? "writable "
        : "non-writable ");
    if (val & PAGE_GLOBAL)
    {
        output = csnprintf(output, end, "global ");
    }
    output = csnprintf(output, end, (val & PAGE_USER)
        ? "user)"
        : "kernel)");

    return orig_output;
}

static const char* pte_print(const pte_t* pte, char* output, size_t size)
{
    const pte_t val = *pte;
    const char* orig_output = output;
    const char* end = output + size;

    output = csnprintf(output, end, "%#lx, flags=(", val & ~PAGE_MASK);
    output = csnprintf(output, end, (val & PAGE_PRESENT)
        ? "present "
        : "non-present ");
    output = csnprintf(output, end, (val & PAGE_RW)
        ? "writable "
        : "non-writable ");
    if (val & PAGE_GLOBAL)
    {
        output = csnprintf(output, end, "global ");
    }
    output = csnprintf(output, end, (val & PAGE_USER)
        ? "user)"
        : "kernel)");

    return orig_output;
}

void page_tables_print(const char* header, loglevel_t severity, uintptr_t address, const pgd_t* pgd)
{
    char buffer[256];
    const pte_t* pte;
    const pmd_t* pmde;
    const pud_t* pude;
    const pgd_t* pgde = pgd_offset(pgd, address);
    const char* next_table_name = "pgd";

    UNUSED(pgd_print); UNUSED(pud_print);

#ifdef PUD_FOLDED
    pude = pgde;
#else
    if (pgd_entry_none(pgde))
    {
        log(severity, "%s: pgd[%zu]: not mapped", header, pgde - pgd);
        return;
    }

    log(severity, "%s: pgd[%zu]: %s",
        header,
        pgde - pgd,
        pgd_print(pgde, buffer, sizeof(buffer)));

    pude = pud_offset(pgde, cr2);
    next_table_name = "pud";
#endif

#ifdef PMD_FOLDED
    pmde = pude;
#else
    if (pud_entry_none(pude))
    {
        log(severity, "%s: %s[%zu]: not mapped", header, next_table_name, pud_index(address));
        return;
    }

    log(severity, "%s: %s[%zu]: %s",
        header,
        next_table_name,
        pud_index(address),
        pud_print(pude, buffer, sizeof(buffer)));

    pmde = pmd_offset(pude, cr2);
#endif

    if (pmd_entry_none(pmde))
    {
        log(severity, "%s: %s[%zu]: not mapped", header, next_table_name, pmd_index(address));
        return;
    }

    log(severity, "%s: %s[%zu]: %s",
        header,
        next_table_name,
        pmd_index(address),
        pmd_print(pmde, buffer, sizeof(buffer)));

    pte = pte_offset(pmde, address);

    if (pte_entry_none(pte))
    {
        log(severity, "%s: pte[%zu]: not mapped", header, pte_index(address));
        return;
    }

    log(severity, "%s: pte[%zu]: %s",
        header,
        pte_index(address),
        pte_print(pte, buffer, sizeof(buffer)));
}
