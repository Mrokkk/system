#include <kernel/vm.h>
#include <kernel/init.h>
#include <kernel/ksyms.h>
#include <kernel/usyms.h>
#include <kernel/reboot.h>
#include <kernel/process.h>
#include <kernel/sections.h>

#include <arch/io.h>
#include <arch/irq.h>
#include <arch/earlycon.h>
#include <arch/register.h>
#include <arch/descriptor.h>

struct exception;
typedef struct exception exception_t;

static void kernel_fault(const exception_t* exception, struct pt_regs* regs);
static void page_fault_description_print(struct pt_regs* regs, uint32_t cr2, uint32_t cr3, const char* header);
static void general_protection_description_print(struct pt_regs* regs, uint32_t, uint32_t, const char* header);

typedef void (*printer_t)(struct pt_regs* regs, uint32_t cr2, uint32_t cr3, const char* header);

struct exception
{
    const char* name;
    const unsigned nr;
    int has_error_code;
    int signal;
};

#define __exception(EXC, NR, EC, NAME, SIG) \
    [NR] = { \
        .name = NAME, \
        .nr = NR, \
        .has_error_code = EC, \
        .signal = SIG, \
    },

#define __exception_debug __exception

const exception_t exceptions[] = {
    #include <arch/exception.h>
};

static int exception_ongoing;

static inline void pf_reason_print(uint32_t error_code, uint32_t cr2, char* output)
{
    output += sprintf(output, (error_code & PF_WRITE)
        ? "Write access "
        : "Read access ");
    output += sprintf(output, (error_code & PF_PRESENT)
            ? "to protected page at virtual address %x "
            : "to non-present page at virtual address %x ",
        cr2);
    output += sprintf(output, (error_code & PF_USER)
        ? "in user space"
        : "in kernel space");
}

static void page_fault_description_print(struct pt_regs* regs, uint32_t cr2, uint32_t cr3, const char* header)
{
    char buffer[256];
    const pgd_t* pgd = virt_cptr(cr3);
    const uint32_t pde_index = pde_index(cr2);
    const uint32_t pte_index = pte_index(cr2);

    pf_reason_print(regs->error_code, cr2, buffer);
    log_exception("%s: %s", header, buffer);
    log_exception("%s: pgd = cr3 = %08x", header, cr3);

    const uint32_t pgt = pgd[pde_index];
    pde_print(pgt, buffer);

    log_exception("%s: pgd[%u] = %s", header, pde_index, buffer);

    const pgt_t* pgt_ptr = virt_cptr(pgt & PAGE_ADDRESS);

    if (!vm_paddr(addr(pgt_ptr), pgd))
    {
        log_exception("%s: pgt not mapped", header);
    }
    else
    {
        const uint32_t pg = pgt_ptr[pte_index];
        pte_print(pg, buffer);
        log_exception("%s: pgt[%u] = %s", header, pte_index, buffer);
    }
}

static void general_protection_description_print(struct pt_regs* regs, uint32_t, uint32_t, const char* header)
{
    log_exception("%s: error code = %x", header, regs->error_code);
}

static inline printer_t printer_get(int nr)
{
    switch (nr)
    {
        case PAGE_FAULT: return &page_fault_description_print;
        case GENERAL_PROTECTION: return &general_protection_description_print;
        default: return NULL;
    }
}

static inline void do_backtrace(struct pt_regs* regs)
{
    char buffer[128];
    uint32_t* esp_ptr;
    uint32_t esp_addr = addr(&regs->esp);
    uint32_t* stack_start = process_current->mm->kernel_stack;

    log_exception("backtrace (guessed):");

    ksym_string(buffer, regs->eip);
    log_exception("%s", buffer);
    for (esp_ptr = ptr(esp_addr); esp_ptr < stack_start; ++esp_ptr)
    {
        if (is_kernel_text(*esp_ptr))
        {
            ksym_string(buffer, *esp_ptr);
            log_exception("%s", buffer);
        }
    }
}

void do_exception(uint32_t nr, struct pt_regs regs)
{
    scoped_irq_lock();

    const exception_t* exception = &exceptions[nr];
    printer_t printer = printer_get(nr);

    if (unlikely(regs.cs != USER_CS))
    {
        kernel_fault(exception, &regs);
        return;
    }

    uint32_t cr2 = cr2_get();
    uint32_t cr3 = cr3_get();
    char header[48];
    sprintf(header, "%s[%u]", process_current->name, process_current->pid);

    if (likely(nr == PAGE_FAULT))
    {
        vm_area_t* area = vm_find(cr2, process_current->mm->vm_areas);
        if (area && area->vm_flags & VM_WRITE)
        {
            log_debug(DEBUG_EXCEPTION, "%s: COW at %x caused by %x",
                header,
                regs.eip,
                cr2);

            if (!vm_copy_on_write(area, process_current->mm->pgd))
            {
                return;
            }
        }
    }

    log_notice("%s: %s #%x at %x",
        header,
        exception->name,
        regs.error_code,
        regs.eip);

    // FIXME: hack for printing the stacktrace and registers if bash crashes (same check
    // is done in elf_load to load symbols. This is caused by super rare crash of bash
    // process after entering the command. EIP points to list_length at bash/list.c:85,
    // but I haven't figured out what's the real RC (yet)
    const int is_bash = false/*!strcmp(process_current->name, "/bin/bash")*/;

    if ((DEBUG_EXCEPTION || is_bash) && printer)
    {
        printer(&regs, cr2, cr3, header);
        regs_print(header, &regs, log_exception);
    }

    if (DEBUG_BTUSER || is_bash)
    {
        backtrace_user(log_notice, &regs, page_virt_ptr(process_current->bin->symbols_pages));
    }

    do_kill(process_current, exception->signal);
}

static void kernel_fault(const exception_t* exception, struct pt_regs* regs)
{
    const char* header = "kernel";
    char string[80];
    uint32_t cr0 = cr0_get();
    uint32_t cr2 = cr2_get();
    uint32_t cr3 = cr3_get();
    uint32_t cr4 = cr4_get();
    printer_t printer = printer_get(exception->nr);

    ++exception_ongoing;

    panic_mode_enter();

    if (exception_ongoing > 1)
    {
        log_critical("%s: %s #%x during another exception handling at %x...",
            header, exception->name, regs->error_code, regs->eip);
        regs_print("kernel", regs, log_critical);
        for (;; halt());
    }

    if (shutdown_in_progress == SHUTDOWN_IN_PROGRESS)
    {
        log_critical("%s: exception during shutdown...", header);
    }

    if (init_in_progress == INIT_IN_PROGRESS)
    {
        log_critical("%s: exception during early init...", header);
    }

    log_critical("%s: %s #%x from %x in pid %u",
        header,
        exception->name,
        regs->error_code,
        regs->eip,
        process_current->pid);

    if (printer)
    {
        printer(regs, cr2, cr3, header);
    }

    regs_print(header, regs, log_critical);

    cr0_bits_string_get(cr0, string);
    log_critical("%s: CR0 = %08x = (%s)", header, cr0, string);
    log_critical("%s: CR2 = %08x", header, cr2, string);
    log_critical("%s: CR3 = %08x", header, cr3);
    cr4_bits_string_get(cr4, string);
    log_critical("%s: CR4 = %08x = (%s)", header, cr4, string);

    backtrace_exception(regs);

    panic_mode_die();

    for (;; halt());
}
