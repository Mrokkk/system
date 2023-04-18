#include <kernel/init.h>
#include <kernel/ksyms.h>
#include <kernel/reboot.h>
#include <kernel/string.h>
#include <kernel/process.h>
#include <kernel/sections.h>

#include <arch/vm.h>
#include <arch/register.h>
#include <arch/descriptor.h>

char* exception_messages[] = {
    #define __exception_noerrno(x) [__NR_##x] = __STRING_##x,
    #define __exception_errno(x) [__NR_##x] = __STRING_##x,
    #define __exception_debug(x) [__NR_##x] = __STRING_##x,
    #include <arch/exception.h>
};

static void page_fault_description_print(struct exception_frame* regs, uint32_t cr2, uint32_t cr3);
static void general_protection_description_print(struct exception_frame* regs, uint32_t cr2, uint32_t cr3);

typedef void (*printer_t)();

printer_t printers[15] = {
    [__NR_page_fault] = page_fault_description_print,
    [__NR_general_protection] = general_protection_description_print,
};

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

static void page_fault_description_print(struct exception_frame* regs, uint32_t cr2, uint32_t cr3)
{
    char buffer[256];
    const pgd_t* pgd = virt_cptr(cr3);
    uint32_t pde_index = pde_index(cr2);
    uint32_t pte_index = pte_index(cr2);

    log_exception("####################### Reason #######################");
    pf_reason_print(regs->error_code, cr2, buffer);
    log_exception("%s", buffer);
    log_exception("####################### Details ######################");
    log_exception("pgd = cr3 = %08x", cr3);

    uint32_t pgt = pgd[pde_index];
    pde_print(pgt, buffer);

    log_exception("pgd[%u] = %s", pde_index, buffer);

    const pgt_t* pgt_ptr = virt_cptr(pgt & PAGE_ADDRESS);

    if (!vm_paddr(addr(pgt_ptr), pgd))
    {
        log_exception("pgt=%x not mapped", pgt_ptr);
    }
    else
    {
        uint32_t pg = pgt_ptr[pte_index];
        pte_print(pg, buffer);
        log_exception("pgt[%u] = %s", pte_index, buffer);
    }
    regs_print(regs, log_exception);
    log_exception("######################################################");
}

static void general_protection_description_print(struct exception_frame* regs, uint32_t, uint32_t)
{
    log_exception("####################### Reason #######################");
    log_exception("N/A");
    log_exception("####################### Details ######################");
    log_exception("Error code = %x", regs->error_code);
    regs_print(regs, log_exception);
    log_exception("######################################################");
}

static inline void do_backtrace(struct exception_frame* regs)
{
    char buffer[128];
#if FRAME_POINTER
    struct stack_frame
    {
        struct stack_frame* next;
        uint32_t ret;
    };

    struct stack_frame last = {.next = ptr(regs->ebp), .ret = regs->eip};
    struct stack_frame* frame = &last;

    log_critical("Backtrace:");
    for (int i = 0; frame && i < 10; ++i)
    {
        if (!kernel_address(addr(frame)) || !is_kernel_text(frame->ret))
        {
            break;
        }
        ksym_string(buffer, frame->ret);
        log_critical("%s", buffer);
        frame = frame->next;
    }
#else
    uint32_t* esp_ptr;
    uint32_t esp_addr = addr(&regs->esp);
    uint32_t* stack_start = process_current->mm->kernel_stack;

    log_critical("Backtrace (guessed):");

    ksym_string(buffer, regs->eip);
    log_critical("%s", buffer);
    for (esp_ptr = ptr(esp_addr); esp_ptr < stack_start; ++esp_ptr)
    {
        if (is_kernel_text(*esp_ptr))
        {
            ksym_string(buffer, *esp_ptr);
            log_critical("%s", buffer);
        }
    }
#endif
}

void do_exception(uint32_t nr, struct exception_frame regs)
{
    flags_t flags;
    uint32_t cr2 = cr2_get();
    uint32_t cr3 = cr3_get();

    irq_save(flags);

    if (unlikely(regs.cs != USER_CS))
    {
        goto kernel_fault;
    }

    if (likely(nr == __NR_page_fault))
    {
        vm_area_t* area = vm_find(cr2, process_current->mm->vm_areas);
        if (area && area->vm_flags & VM_WRITE)
        {
            log_debug(DEBUG_EXCEPTION, "COW in process %u at %x", process_current->pid, cr2);
            if (!vm_copy_on_write(area))
            {
                irq_restore(flags);
                return;
            }
        }
    }

    if (DEBUG_EXCEPTION && printers[nr])
    {
        log_exception("%s #%x in process %d (%s) at %x",
            exception_messages[nr],
            regs.error_code,
            process_current->pid,
            process_current->name,
            regs.eip);

        printers[nr](&regs, cr2, cr3);
    }

    irq_restore(flags);

    log_notice("sending SIGSEGV to %d:%s", process_current->pid, process_current->name);

    do_kill(process_current, SIGSEGV);
    return;

kernel_fault:
    char string[80];
    uint32_t cr0 = cr0_get();
    uint32_t cr4 = cr4_get();

    ensure_printk_will_print();

    if (shutdown_in_progress == SHUTDOWN_IN_PROGRESS)
    {
        log_critical("Exception during shutdown...");
    }
    if (init_in_progress == INIT_IN_PROGRESS)
    {
        log_critical("Exception during early init...");
    }

    log_critical("%s #%x from %x in pid %u",
        exception_messages[nr],
        regs.error_code,
        regs.eip,
        process_current->pid);

    if (printers[nr])
    {
        printers[nr](&regs, cr2, cr3);
    }

    cr0_bits_string_get(cr0, string);
    log_critical("CR0=%x : %s", cr0, string);
    log_critical("CR2=%x", cr2);
    log_critical("CR3=%x", cr3);
    log_critical("CR4=%x", cr4);

    do_backtrace(&regs);

    for (;; halt());
}
