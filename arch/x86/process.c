#include <arch/processor.h>
#include <arch/descriptor.h>
#include <arch/string.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/process.h>

#define PROCESS_KERNEL_STACK_SIZE 4096

#define push(val, stack) \
    stack--; *stack = (typeof(*stack))(val);

extern void ret_from_syscall();

/*===========================================================================*
 *                                stack_copy                                 *
 *===========================================================================*/
static void *stack_copy(unsigned int *dest, unsigned int *src, unsigned int size) {

    /*
     * New, quite safe copying stacks. Last one was buggy...
     */

    void *dest_begin = (void *)((unsigned int)dest - size);
    void *src_begin = (void *)((unsigned int)src - size);

    return memcpy(dest_begin, src_begin, size);

}

/*===========================================================================*
 *                               process_copy                                *
 *===========================================================================*/
int process_copy(struct process *dest, struct process *src,
                 unsigned int *new_stack, struct pt_regs *old_regs) {

    unsigned int *old_stack = (unsigned int *)src->mm.end;
    unsigned int *stack;

    ASSERT(old_regs->cs == KERNEL_CS || old_regs->cs == USER_CS);
    ASSERT(old_regs->ds == KERNEL_DS || old_regs->ds == USER_DS);
    ASSERT(old_regs->ss == KERNEL_DS || old_regs->ss == USER_DS);
    ASSERT(old_regs->gs == USER_DS);

    if (old_regs->cs == KERNEL_CS)
        dest->kernel = 1;

    memset(&dest->context, 0, sizeof(struct context));

    /* Copy stack */
    if (!dest->kernel)
        new_stack = stack_copy(new_stack, old_stack,
                (unsigned int)old_stack - old_regs->esp);

    dest->context.iomap_offset = 104;
    dest->context.ss0 = KERNEL_DS;
    dest->context.esp0 = (unsigned int)kmalloc(PROCESS_KERNEL_STACK_SIZE) +
            PROCESS_KERNEL_STACK_SIZE;

    stack = (unsigned int *)dest->context.esp0;

    /* Setup kernel stack which should be loaded at context switch */
    push(old_regs->ss, stack);                          /* ss */
    push(new_stack, stack);                             /* esp */
    push(old_regs->eflags | 0x200, stack);              /* eflags */
    push(old_regs->cs, stack);                          /* cs */
    push(old_regs->eip, stack);                         /* eip */
    push(old_regs->gs, stack);                          /* gs */
    push(old_regs->fs, stack);                          /* fs */
    push(old_regs->es, stack);                          /* es */
    push(old_regs->ds, stack);                          /* ds */
    push(0, stack);                                     /* eax */
    push((unsigned int)new_stack + old_regs->ebp
            - (unsigned int)src->context.esp, stack);   /* ebp */
    push(old_regs->edi, stack);                         /* edi */
    push(old_regs->esi, stack);                         /* esi */
    push(old_regs->edx, stack);                         /* edx */
    push(old_regs->ecx, stack);                         /* ecx */
    push(old_regs->ebx, stack);                         /* ebx */

    dest->context.esp = (unsigned int)stack;
    dest->context.eip = (unsigned int)&ret_from_syscall;

    /* Copy ports permissions */
    memcpy(dest->context.io_bitmap, src->context.io_bitmap, 128);

    return 0;

}

/*===========================================================================*
 *                            arch_process_free                              *
 *===========================================================================*/
void arch_process_free(struct process *proc) {

    /* Free kernel stack */
    kfree((void *)((unsigned int)proc->context.esp0 -
            PROCESS_KERNEL_STACK_SIZE));

}

/*===========================================================================*
 *                          arch_kthread_regs_init                           *
 *===========================================================================*/
int arch_kthread_regs_init(struct pt_regs *regs, unsigned int ip) {

    ASSERT(regs != 0);
    ASSERT(ip != 0);

    memset(regs, 0, sizeof(struct pt_regs));

    regs->cs = KERNEL_CS;
    regs->ds = KERNEL_DS;
    regs->ebp = process_current->context.esp;
    regs->eip = ip;
    regs->ss = KERNEL_DS;
    regs->es = KERNEL_DS;
    regs->gs = USER_DS;
    regs->fs = KERNEL_DS;

    return 0;

}

#define FASTCALL(x) __attribute__((regparm(3))) x

/*===========================================================================*
 *                             __process_switch                              *
 *===========================================================================*/
void FASTCALL(__process_switch(struct process *prev, struct process *next)) {

    unsigned int base = (unsigned int)&next->context;

    ASSERT(prev != 0);
    ASSERT(next != 0);

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
