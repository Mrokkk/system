#include <arch/processor.h>
#include <arch/descriptor.h>
#include <arch/string.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/io.h>
#include <kernel/process.h>

#define PROCESS_KERNEL_STACK_SIZE 4096

#define push(val, stack) \
    stack--; *stack = (typeof(*stack))(val);

extern void ret_from_syscall();

/*===========================================================================*
 *                                stack_copy                                 *
 *===========================================================================*/
static void *stack_copy(unsigned int *dest, unsigned int *src,
        unsigned int size) {

    /*
     * New, quite safe copying stacks. Last one was buggy...
     */

    void *dest_begin = (void *)((unsigned int)dest - size);
    void *src_begin = (void *)((unsigned int)src - size);

    return memcpy(dest_begin, src_begin, size);

}

/*===========================================================================*
 *                              stack_create                                 *
 *===========================================================================*/
static inline void *stack_create(unsigned int size) {

    return (void *)((unsigned int)kmalloc(size) + size);

}

/* I'm sure *_process_setup_stack functions can look better,
 * but I leave it as it is.
 */

/*===========================================================================*
 *                        kernel_process_setup_stack                         *
 *===========================================================================*/
unsigned int kernel_process_setup_stack(struct process *dest,
        struct process *src,
        struct pt_regs *src_regs) {

    unsigned int *dest_stack = (unsigned int *)dest->mm.end,
                 *process_stack;

    /*
     * We're not creating kernel stack for kernel process,
     * because it would never be used (except for one case)
     */

    (void)src;

    /*
     * If we'are not changing privilege level, which we don't do in that case
     * (privilege levels in the process and in the handler are the same), we
     * don't have the EIP value in the pt_regs (exactly we have but it points
     * to some random value). But we can figure out its address. If CPU doesn't
     * change privileges in the exception entry, it doesn't change stacks, so
     * it simply pushes appropriate values (EFLAGS, CS and EIP) on the
     * currently using stack. So, the ESP value that has been used in the
     * process before exception is the address 4 bytes after EFLAGS, which
     * is the address of the ESP field in the pt_regs.
     */

    dest_stack = stack_copy(
            (unsigned int *)dest_stack,
            (unsigned int *)src->mm.end,
            (unsigned int)src->mm.end - (unsigned int)&src_regs->esp);

    process_stack = dest_stack;
    dest->context.esp0 = 0;

    push(src_regs->eflags | 0x200, dest_stack);             /* eflags */
    push(KERNEL_CS, dest_stack);                            /* cs */
    push(src_regs->eip, dest_stack);                        /* eip */
    push(src_regs->gs, dest_stack);                         /* gs */
    push(src_regs->fs, dest_stack);                         /* fs */
    push(src_regs->es, dest_stack);                         /* es */
    push(src_regs->ds, dest_stack);                         /* ds */
    push(0, dest_stack);                                    /* eax */
    push((unsigned int)process_stack + src_regs->ebp
            - (unsigned int)src->context.esp, dest_stack);  /* ebp */
    push(src_regs->edi, dest_stack);                        /* edi */
    push(src_regs->esi, dest_stack);                        /* esi */
    push(src_regs->edx, dest_stack);                        /* edx */
    push(src_regs->ecx, dest_stack);                        /* ecx */
    push(src_regs->ebx, dest_stack);                        /* ebx */

    return (unsigned int)dest_stack;

}

/*===========================================================================*
 *                         user_process_setup_stack                          *
 *===========================================================================*/
unsigned int user_process_setup_stack(struct process *dest,
        struct process *src,
        struct pt_regs *src_regs) {

    unsigned int *kernel_stack;
    unsigned int *user_stack;
    unsigned int *dest_stack_start = (unsigned int *)dest->mm.end;
    unsigned int *src_stack_start = (unsigned int *)src->mm.end;
    unsigned int *src_stack_end = (unsigned int *)src_regs->esp;

    dest->context.esp0 = (unsigned int)(kernel_stack =
            stack_create(PROCESS_KERNEL_STACK_SIZE));

    user_stack = stack_copy((unsigned int *)dest_stack_start, src_stack_start,
            (unsigned int)src_stack_start - (unsigned int)src_stack_end);

    /* Setup kernel stack which should be loaded at context switch */
    push(USER_DS, kernel_stack);                                /* ss */
    push(user_stack, kernel_stack);                             /* esp */
    push(src_regs->eflags | 0x200, kernel_stack);               /* eflags */
    push(USER_CS, kernel_stack);                                /* cs */
    push(src_regs->eip, kernel_stack);                          /* eip */
    push(src_regs->gs, kernel_stack);                           /* gs */
    push(src_regs->fs, kernel_stack);                           /* fs */
    push(src_regs->es, kernel_stack);                           /* es */
    push(src_regs->ds, kernel_stack);                           /* ds */
    push(0, kernel_stack);                                      /* eax */
    push((unsigned int)user_stack + src_regs->ebp
            - (unsigned int)src->context.esp, kernel_stack);    /* ebp */
    push(src_regs->edi, kernel_stack);                          /* edi */
    push(src_regs->esi, kernel_stack);                          /* esi */
    push(src_regs->edx, kernel_stack);                          /* edx */
    push(src_regs->ecx, kernel_stack);                          /* ecx */
    push(src_regs->ebx, kernel_stack);                          /* ebx */

    return (unsigned int)kernel_stack;

}

/*===========================================================================*
 *                             arch_process_copy                             *
 *===========================================================================*/
int arch_process_copy(struct process *dest, struct process *src,
        struct pt_regs *src_regs) {

    if (process_is_kernel(dest))
        dest->context.esp = kernel_process_setup_stack(dest, src, src_regs);
    else
        dest->context.esp = user_process_setup_stack(dest, src, src_regs);

    dest->context.eip = (unsigned int)&ret_from_syscall;
    dest->context.iomap_offset = 104;
    dest->context.ss0 = KERNEL_DS;

    /* Copy ports permissions */
    memcpy(dest->context.io_bitmap, src->context.io_bitmap, 128);

    return 0;

}

/*===========================================================================*
 *                             arch_process_free                             *
 *===========================================================================*/
void arch_process_free(struct process *proc) {

    /* Free kernel stack (if exists) */
    if (proc->context.esp0)
        kfree((void *)((unsigned int)proc->context.esp0 -
            PROCESS_KERNEL_STACK_SIZE));

}

#define FASTCALL(x) __attribute__((regparm(3))) x

/*===========================================================================*
 *                             __process_switch                              *
 *===========================================================================*/
void FASTCALL(__process_switch(struct process *prev, struct process *next)) {

    unsigned int base = (unsigned int)&next->context;

    /*
     * Simply reload TSS with the address of process's context
     * structure
     */

    (void)prev; (void)next;

    descriptor_set_base(gdt_entries, FIRST_TSS_ENTRY, base);

    gdt_entries[FIRST_TSS_ENTRY].access &= 0xf9; /* Clear busy bit */

    tss_load(FIRST_TSS_ENTRY << 3);

}

/*===========================================================================*
 *                                regs_print                                 *
 *===========================================================================*/
void regs_print(struct pt_regs *regs) {

    printk("EAX=0x%08x; ", regs->eax);
    printk("EBX=0x%08x; ", regs->ebx);
    printk("ECX=0x%08x; ", regs->ecx);
    printk("EDX=0x%08x\n", regs->edx);
    printk("ESP=0x%04x:0x%08x; ", regs->ss, regs->esp);
    printk("EIP=0x%04x:0x%08x\n", regs->cs, regs->eip);
    printk("DS=0x%04x; ES=0x%04x; FS=0x%04x; GS=0x%04x\n",
            regs->ds, regs->es, regs->fs, regs->gs);
    printk("EFLAGS=0x%08x\n", regs->eflags);

}

#define set_context(stack, ip) \
    do {                        \
        asm volatile(           \
            "movl %0, %%esp;"   \
            "jmp *%1;"          \
            :: "r" (stack),     \
               "r" (ip));       \
    } while (0)

/*===========================================================================*
 *                                  sys_exec                                 *
 *===========================================================================*/
__noreturn int sys_exec(struct pt_regs regs) {

    unsigned int *kernel_stack,
                 *user_stack;
    unsigned int flags, eip = 0;

    save_flags(flags);
    cli();

    eip = regs.ebx;

    if (process_is_kernel(process_current)) {
        /* We change kernel process to the user process. To do so,
         * we must create the kernel stack for it.
         */
        kernel_stack = stack_create(PROCESS_KERNEL_STACK_SIZE);
        process_current->context.esp0 = (unsigned int)kernel_stack;
    } else kernel_stack = (unsigned int *)process_current->context.esp0;

    process_current->kernel = 0;

    user_stack = process_current->mm.end;

    push(USER_DS, kernel_stack);                    /* ss */
    push(user_stack, kernel_stack);                 /* esp */
    push(0x200, kernel_stack);                      /* eflags */
    push(USER_CS, kernel_stack);                    /* cs */
    push(eip, kernel_stack);                        /* eip */
    push(USER_DS, kernel_stack);                    /* gs */
    push(USER_DS, kernel_stack);                    /* fs */
    push(USER_DS, kernel_stack);                    /* es */
    push(USER_DS, kernel_stack);                    /* ds */
    push(0, kernel_stack);                          /* eax */
    kernel_stack -= 6;

    restore_flags(flags);

    set_context(kernel_stack, &ret_from_syscall);

    while (1);

}
