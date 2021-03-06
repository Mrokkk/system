#include <kernel/kernel.h>
#include <kernel/elf.h>
#include <kernel/process.h>
#include <kernel/irq.h>
#include <kernel/mm.h>
#include <kernel/string.h>

#include <arch/processor.h>
#include <arch/system.h>
#include <arch/descriptor.h>
#include <arch/cpuid.h>
#include <arch/register.h>
#include <arch/segment.h>

char *exception_messages[] = {
    #define __exception_noerrno(x) [__NR_##x] = __STRING_##x,
    #define __exception_errno(x) [__NR_##x] = __STRING_##x,
    #define __exception_debug(x) [__NR_##x] = __STRING_##x,
    #include <arch/exception.h>
};

void do_exception(
    unsigned long nr,       /* Exception number */
    int error_code,         /* Error code */
    struct pt_regs regs     /* Registers */
) {

    struct kernel_symbol *symbol;
    char *function;
    char string[80];
    unsigned int cr0 = cr0_get(),
                 cr2 = cr2_get(),
                 cr3 = cr3_get(),
                 cr4 = cr4_get();

    cli();

    symbol = symbol_find_address(regs.eip);
    function = symbol ? symbol->name : "<unknown>";

    /* If we are in user space, kill process and resched */
    if (regs.cs == USER_CS) {
        printk("%s #0x%x in process %d (%s) at 0x%x (%s)\n",
                exception_messages[nr],
                error_code,
                process_current->pid,
                process_current->name,
                (unsigned int)regs.eip,
                function);

        if (nr == __NR_page_fault) {
            printk("cr2 = 0x%x\n", cr2);
            printk("cr3 = 0x%x\n", cr3);
        }

        process_exit(process_current);
    }

    regs_print(&regs);

    cr0_bits_string_get(cr0, string);
    printk("CR0=0x%x : %s\n", (unsigned int)cr0, string);

    printk("CR2=0x%x; ", (unsigned int)cr2);
    printk("CR3=0x%x; ", (unsigned int)cr3);
    printk("CR4=0x%x\n", (unsigned int)cr4);
    panic("%s #0x%x", exception_messages[nr], error_code);

    while (1);

}


