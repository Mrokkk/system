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
#include <arch/opcode.h>
#include <arch/register.h>
#include <arch/segment.h>

char *exception_messages[] = {
    #define __exception_noerrno(x) [__NR_##x] = __STRING_##x,
    #define __exception_errno(x) [__NR_##x] = __STRING_##x,
    #define __exception_debug(x) [__NR_##x] = __STRING_##x,
    #include <arch/exception.h>
};

/*===========================================================================*
 *                               do_exception                                *
 *===========================================================================*/
void do_exception(
    unsigned long nr,       /* Exception number */
    int error_code,         /* Error code */
    struct pt_regs *regs    /* Registers */
) {

    struct opcode_decoded_struct decoded;
    char string[80];
    unsigned int cr0 = cr0_get(),
                 cr2 = cr2_get(),
                 cr3 = cr3_get(),
                 cr4 = cr4_get();

    opcode_decode((unsigned char *)regs->eip, &decoded);

    if (nr == __NR_debug) { /* TODO: Debug */

        printk("Instruction: 0x%08x : %s\n", regs->eip, decoded.string);

        return;

    }

    /* If we are in user space, kill process and resched */
    if (regs->cs == USER_CS) {
        printk("%s in process %d (%s) at 0x%x :: %s\n",
                exception_messages[nr],
                process_current->pid,
                process_current->name,
                (unsigned int)regs->eip,
                decoded.string);

        process_exit(process_current);
        while (1);
    }

    printk("Exception: %s #0x%x\n", exception_messages[nr], error_code);
    regs_print(regs);
    printk("IOPL=%d; ", ((unsigned int)regs->eflags >> 12) & 0x3);
    eflags_bits_string_get(regs->eflags, string);
    printk("%s\n", string);
    printk("Instruction: %s\n", decoded.string);

    printk("CR0=0x%x\n", (unsigned int)cr0);
    cr0_bits_string_get(cr0, string);
    printk("  %s\n", string);

    printk("CR2=0x%x; ", (unsigned int)cr2);
    printk("CR3=0x%x; ", (unsigned int)cr3);
    printk("CR4=0x%x\n", (unsigned int)cr4);
    cr4_bits_string_get(cr4, string);
    printk("  %s\n", string);

    if (regs->eip < 0x100000) {
        printk("Bad instruction pointer due to stack corruption or jumping to bad function pointer\n");
    }

    printk("\nAttempting to reset in 7 seconds...");

    delay(7000);

    reboot();

    while (1);

}


