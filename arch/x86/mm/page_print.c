#include <kernel/kernel.h>
#include <kernel/page_table.h>

void pgd_print(const pgd_t* pgd, char* output, size_t size)
{
    const pgd_t val = *pgd;
    const char* end = output + size;
    output = csnprintf(output, end, "%lx, flags=(", val & ~PAGE_MASK);
    output = csnprintf(output, end, (val & PAGE_PRESENT)
        ? "present "
        : "non-present ");
    output = csnprintf(output, end, (val & PAGE_RW)
        ? "writable "
        : "non-writable ");
    output = csnprintf(output, end, (val & PAGE_USER)
        ? "user)"
        : "kernel)");
}

void pud_print(const pud_t* pud, char* output, size_t size)
{
    const pud_t val = *pud;
    const char* end = output + size;
    output = csnprintf(output, end, "%lx, flags=(", val & ~PAGE_MASK);
    output = csnprintf(output, end, (val & PAGE_PRESENT)
        ? "present "
        : "non-present ");
    output = csnprintf(output, end, (val & PAGE_RW)
        ? "writable "
        : "non-writable ");
    output = csnprintf(output, end, (val & PAGE_USER)
        ? "user)"
        : "kernel)");
}

void pmd_print(const pmd_t* pmd, char* output, size_t size)
{
    const pmd_t val = *pmd;
    const char* end = output + size;
    output = csnprintf(output, end, "%lx, flags=(", val & ~PAGE_MASK);
    output = csnprintf(output, end, (val & PAGE_PRESENT)
        ? "present "
        : "non-present ");
    output = csnprintf(output, end, (val & PAGE_RW)
        ? "writable "
        : "non-writable ");
    output = csnprintf(output, end, (val & PAGE_USER)
        ? "user)"
        : "kernel)");
}

void pte_print(const pte_t* pte, char* output, size_t size)
{
    const pte_t val = *pte;
    const char* end = output + size;
    output = csnprintf(output, end, "%lx, flags=(", val & ~PAGE_MASK);
    output = csnprintf(output, end, (val & PAGE_PRESENT)
        ? "present "
        : "non-present ");
    output = csnprintf(output, end, (val & PAGE_RW)
        ? "writable "
        : "non-writable ");
    output = csnprintf(output, end, (val & PAGE_USER)
        ? "user)"
        : "kernel)");
}
