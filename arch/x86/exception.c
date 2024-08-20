#include <kernel/vm.h>
#include <kernel/init.h>
#include <kernel/page.h>
#include <kernel/ksyms.h>
#include <kernel/sysfs.h>
#include <kernel/reboot.h>
#include <kernel/process.h>
#include <kernel/sections.h>
#include <kernel/vm_print.h>

#include <arch/io.h>
#include <arch/irq.h>
#include <arch/earlycon.h>
#include <arch/register.h>
#include <arch/descriptor.h>

#define DEBUG_PAGE_FAULT        0
#define DEBUG_PAGE_FAULT_TRACE  0

#if DEBUG_PAGE_FAULT_TRACE
#define DEBUG_PAGE_FAULT_EIP    0 // Define EIP to be traced
#define DEBUG_PAGE_FAULT_CR2    0 // Define CR2 to be traced
#endif

struct exception
{
    const char* name;
    const unsigned nr;
    const int has_error_code;
    const int signal;
};

typedef struct exception exception_t;
typedef void (*printer_t)(loglevel_t severity, const pt_regs_t* regs, uint32_t cr2, uint32_t cr3, const char* header);

static void NORETURN(kernel_fault(const exception_t* exception, const pt_regs_t* regs, const printer_t printer));
static void page_fault_description_print(loglevel_t severity, const pt_regs_t* regs, uint32_t cr2, uint32_t cr3, const char* header);
static void general_protection_description_print(loglevel_t severity, const pt_regs_t* regs, uint32_t, uint32_t, const char* header);

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
static uintptr_t prev_cr2;
static bool prev_cr2_valid;

extern uintptr_t __stack_chk_guard;
uintptr_t __stack_chk_guard __attribute__((used));

void NORETURN(__stack_chk_fail(void))
{
    panic("Stack smashing detected!");
}

static void pf_reason_print(uint32_t error_code, uint32_t cr2, char* output, size_t size)
{
    const char* end = output + size;
    output = csnprintf(output, end, (error_code & PF_WRITE)
        ? "Write access "
        : "Read access ");
    output = csnprintf(output, end, (error_code & PF_PRESENT)
            ? "to protected page at virtual address %x "
            : "to non-present page at virtual address %x ",
        cr2);
    output = csnprintf(output, end, (error_code & PF_USER)
        ? "in user space"
        : "in kernel space");
}

static void page_fault_description_print(loglevel_t severity, const pt_regs_t* regs, uint32_t cr2, uint32_t cr3, const char* header)
{
    char buffer[256];
    const pgd_t* pgd = virt_cptr(cr3);
    const uint32_t pde_index = pde_index(cr2);
    const uint32_t pte_index = pte_index(cr2);

    pf_reason_print(regs->error_code, cr2, buffer, sizeof(buffer));
    log(severity, "%s: %s", header, buffer);
    log(severity, "%s: pgd: cr3 = %08x", header, cr3);

    const uint32_t pgt = pgd[pde_index];
    pde_print(pgt, buffer, sizeof(buffer));

    log(severity, "%s: pgd[%u]: %s", header, pde_index, buffer);

    const pgt_t* pgt_ptr = virt_cptr(pgt & PAGE_ADDRESS);

    if (!vm_paddr(addr(pgt_ptr), pgd))
    {
        log(severity, "%s: pgt: not mapped", header);
    }
    else
    {
        const uint32_t pg = pgt_ptr[pte_index];
        pte_print(pg, buffer, sizeof(buffer));
        log(severity, "%s: pgt[%u]: %s", header, pte_index, buffer);
    }
}

static void general_protection_description_print(loglevel_t severity, const pt_regs_t* regs, uint32_t, uint32_t, const char* header)
{
    log(severity, "%s: error code: %x", header, regs->error_code);
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

void do_exception(uint32_t nr, pt_regs_t regs)
{
    char header[48];
    vm_area_t* vma;
    process_t* p = process_current;

    scoped_irq_lock();

    uint32_t cr2 = cr2_get();
    uint32_t cr3 = cr3_get();

    if (unlikely(nr != PAGE_FAULT))
    {
        goto handle_fault;
    }

    if (DEBUG_PAGE_FAULT_TRACE)
    {
        if (
#ifdef DEBUG_PAGE_FAULT_EIP
            regs.eip == DEBUG_PAGE_FAULT_EIP ||
#endif
#ifdef DEBUG_PAGE_FAULT_CR2
            cr2 == DEBUG_PAGE_FAULT_CR2 ||
#endif
            0)
        {
            uintptr_t paddr = vm_paddr(cr2, p->mm->pgd);
            current_log_info("page fault from %x caused by %x (paddr %x)", regs.eip, cr2, paddr);
        }
    }

    // FIXME
    // There's a random "crash" of snake. It's not really a crash, but rather a page fault
    // with stale CR2 from previous page fault in bash (valid COW case). My first hypothesis
    // was that timer fires between exc_page_fault_handler entry and cli above and it seems
    // to be partially correct. After saving jiffies and CR2 to process_t in SAVE_ALL
    // (SAVE_ALL also started doing cli as the first instruction and sti after saving data
    // to process_t) and printing it here I was able to see:
    // [     112.595563] /bin/bash[2]: pf from 0xdc8e4, jiffies 11129, proc cr2 = 0xdc8e4, proc jiffies 11129
    // [     112.602736] /bin/snake[35]: pf from 0xdc8e4, jiffies 11129, proc cr2 = 0xbf818800, proc jiffies 11128
    //
    // So in above case page fault for 0xbf818800 from snake was the first one, but got
    // interrupted due to timer IRQ, which ran bash, which then triggered a page fault for
    // 0xdc8e4. When CPU was taken from bash, snake was restarted at the exception handling
    // and it read the stale CR2.
    //
    // But I was also able to reproduce following situation:
    // [      29.005790] /bin/bash[2]: pf from 0xdc8e4, jiffies 2770, proc cr2 = 0xdc8e4, proc jiffies 2770
    // [      29.011290] /bin/snake[22]: pf from 0xdc8e4, jiffies 2770, proc cr2 = 0xdc8e4, proc jiffies 2770
    //
    // In above case jiffies and proc jiffies are the same which means that there was no timer
    // IRQ between cli in SAVE_ALL and cli above. It might mean that either:
    // - there's IRQ firing just before calling cli in SAVE_ALL (very unlikely)
    // - there's some other race condition or some misuse of MMU
    //
    // Until I figure out how to fix it properly, below WA is simply restarting page fault if
    // previous page fault came from the same address
    if (prev_cr2_valid && cr2 == prev_cr2)
    {
        prev_cr2_valid = false;
        return;
    }

    prev_cr2 = cr2;
    prev_cr2_valid = true;

    vma = vm_find(cr2, p->mm->vm_areas);

    if (unlikely(!vma))
    {
        goto handle_fault;
    }

    current_log_debug(DEBUG_PAGE_FAULT, "page fault at %x caused by access to %x", regs.eip, cr2);

    if (unlikely(vm_nopage(vma, p->mm->pgd, cr2, regs.error_code & PF_WRITE)))
    {
        goto handle_fault;
    }

    return;

handle_fault:
    {
        const exception_t* exception = &exceptions[nr];
        printer_t printer = printer_get(nr);

        if (unlikely(regs.cs != USER_CS))
        {
            kernel_fault(exception, &regs, printer);
        }

        snprintf(header, sizeof(header), "%s[%u]", p->name, p->pid);
        log_info("%s: %s #%x at %x",
            header,
            exception->name,
            exception->has_error_code ? regs.error_code : 0,
            regs.eip);

        if (sys_config.user_backtrace)
        {
            if (printer)
            {
                printer(KERN_INFO, &regs, cr2, cr3, header);
            }

            regs_print(KERN_INFO, &regs, header);
            vm_areas_indent_log(KERN_INFO, p->mm->vm_areas, INDENT_LVL_1, "%s: vm areas:", header);
            backtrace_user(KERN_INFO, &regs, "");
        }

        do_kill(p, exception->signal);
    }
}

static void NORETURN(kernel_fault(const exception_t* exception, const pt_regs_t* regs, const printer_t printer))
{
    const char* header = "kernel";
    char string[80];
    uint32_t cr0 = cr0_get();
    uint32_t cr2 = cr2_get();
    uint32_t cr3 = cr3_get();
    uint32_t cr4 = cr4_get();
    process_t* p = process_current;

    ++exception_ongoing;

    pgd_load(init_pgd_get());

    panic_mode_enter();

    if (exception_ongoing > 1)
    {
        log_critical("%s: %s #%x during another exception handling at %x...",
            header, exception->name, regs->error_code, regs->eip);
        regs_print(KERN_CRIT, regs, "kernel");
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
        exception->has_error_code ? regs->error_code : 0,
        regs->eip,
        p->pid);

    if (printer)
    {
        printer(KERN_CRIT, regs, cr2, cr3, header);
    }

    regs_print(KERN_CRIT, regs, header);
    log_critical("%s: cr0: %08x: (%s)", header, cr0, cr0_bits_string_get(cr0, string, sizeof(string)));
    log_critical("%s: cr2: %08x cr3: %08x", header, cr2, cr3);
    log_critical("%s: cr4: %08x: (%s)", header, cr4, cr4_bits_string_get(cr4, string, sizeof(string)));

    backtrace_exception(regs);

    panic_mode_die();

    for (;; halt());
}
