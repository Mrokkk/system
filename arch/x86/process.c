#include <arch/processor.h>
#include <arch/descriptor.h>
#include <arch/string.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/io.h>
#include <arch/page.h>

#include <kernel/unistd.h>
#include <kernel/process.h>

#define PROCESS_KERNEL_STACK_SIZE 4096

#define push(val, stack) \
    (stack)--; *stack = (typeof(*stack))(val);

extern void ret_from_syscall();

static void *stack_copy(unsigned int *dest, unsigned int *src,
        unsigned int size) {

    void *dest_begin = (void *)((unsigned int)dest - size);
    void *src_begin = (void *)((unsigned int)src - size);

    return memcpy(dest_begin, src_begin, size);

}

static inline void *stack_create(unsigned int size) {

    (void)size;

    return (void *)((unsigned int)page_alloc() + PAGE_SIZE);

}

static inline void fork_kernel_stack_frame(unsigned int **kernel_stack,
        unsigned int *user_stack, struct pt_regs *regs) {

    if (regs->cs == USER_CS) {
        push(USER_DS, *kernel_stack);                   /* ss */
        push(user_stack, *kernel_stack);                /* esp */
    }
    push(regs->eflags | 0x200, *kernel_stack);          /* eflags */
    push(regs->cs, *kernel_stack);                      /* cs */
    push(regs->eip, *kernel_stack);                     /* eip */
    push(regs->gs, *kernel_stack);                      /* gs */
    push(regs->fs, *kernel_stack);                      /* fs */
    push(regs->es, *kernel_stack);                      /* es */
    push(regs->ds, *kernel_stack);                      /* ds */
    push(0, *kernel_stack);                             /* eax */
    push((unsigned int)user_stack + regs->ebp
            - (unsigned int)regs->esp, *kernel_stack);  /* ebp */
    push(regs->edi, *kernel_stack);                     /* edi */
    push(regs->esi, *kernel_stack);                     /* esi */
    push(regs->edx, *kernel_stack);                     /* edx */
    push(regs->ecx, *kernel_stack);                     /* ecx */
    push(regs->ebx, *kernel_stack);                     /* ebx */

}

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

    fork_kernel_stack_frame(&dest_stack, process_stack, src_regs);

    return (unsigned int)dest_stack;

}

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

    fork_kernel_stack_frame(&kernel_stack, user_stack, src_regs);

    return (unsigned int)kernel_stack;

}

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

void arch_process_free(struct process *proc) {

    /* Free kernel stack (if exists) */
    if (proc->context.esp0)
        page_free((void *)((unsigned int)proc->context.esp0 -
            PAGE_SIZE));

}

#define FASTCALL(x) __attribute__((regparm(3))) x

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

void syscall_regs_check(struct pt_regs regs) {

    ASSERT(cs_get() == KERNEL_CS);
    ASSERT(ds_get() == KERNEL_DS);
    if (process_is_kernel(process_current)) {
        ASSERT(regs.cs == KERNEL_CS);
        ASSERT(regs.ds == KERNEL_DS);
        ASSERT(regs.gs == KERNEL_DS);
        ASSERT(gs_get() == KERNEL_DS);
        ASSERT(regs.gs == KERNEL_DS);
    } else {
        ASSERT(regs.cs == USER_CS);
        ASSERT(regs.ds == USER_DS);
        ASSERT(regs.gs == USER_DS);
        ASSERT(regs.gs == USER_DS);
        ASSERT(gs_get() == USER_DS);
    }

}

void regs_print(struct pt_regs *regs) {

    char string[64];

    printk("EAX=0x%08x; ", regs->eax);
    printk("EBX=0x%08x; ", regs->ebx);
    printk("ECX=0x%08x; ", regs->ecx);
    printk("EDX=0x%08x\n", regs->edx);
    printk("ESP=0x%04x:0x%08x; ", regs->ss, regs->esp);
    printk("EIP=0x%04x:0x%08x\n", regs->cs, regs->eip);
    printk("DS=0x%04x; ES=0x%04x; FS=0x%04x; GS=0x%04x\n",
            regs->ds, regs->es, regs->fs, regs->gs);
    printk("EFLAGS=0x%08x : ", regs->eflags);

    eflags_bits_string_get(regs->eflags, string);
    printk("IOPL=%d ", ((unsigned int)regs->eflags >> 12) & 0x3);
    printk("%s\n", string);

}

int sys_clone(struct pt_regs regs) {

    unsigned int sp = regs.ecx;

    if (!sp) sp = regs.esp;

    return process_clone(process_current, &regs, regs.ebx);

}

static inline void exec_kernel_stack_frame(
        unsigned int **kernel_stack, unsigned int esp, unsigned int eip) {

    push(USER_DS, *kernel_stack);   /* ss */
    push(esp, *kernel_stack);       /* esp */
    push(0x200, *kernel_stack);     /* eflags */
    push(USER_CS, *kernel_stack);   /* cs */
    push(eip, *kernel_stack);       /* eip */
    push(USER_DS, *kernel_stack);   /* gs */
    push(USER_DS, *kernel_stack);   /* fs */
    push(USER_DS, *kernel_stack);   /* es */
    push(USER_DS, *kernel_stack);   /* ds */
    push(0, *kernel_stack);         /* eax */
    *kernel_stack -= 6;

}

#define set_context(stack, ip) \
    do {                        \
        asm volatile(           \
            "movl %0, %%esp;"   \
            "jmp *%1;"          \
            :: "r" (stack),     \
               "r" (ip));       \
    } while (0)

__noreturn int sys_exec(struct pt_regs regs) {

    unsigned int *kernel_stack, *user_stack, flags, eip = 0;

    irq_save(flags);

    eip = regs.ebx;

    if (process_is_kernel(process_current)) {
        /* We change kernel process to the user process. To do so,
         * we must create the kernel stack for it.
         */
        kernel_stack = stack_create(PROCESS_KERNEL_STACK_SIZE);
        process_current->context.esp0 = (unsigned int)kernel_stack;
    } else kernel_stack = (unsigned int *)process_current->context.esp0;

    process_current->type = 0;

    user_stack = process_current->mm.end;

    exec_kernel_stack_frame(&kernel_stack, (unsigned int)user_stack, eip);

    irq_restore(flags);

    set_context(kernel_stack, &ret_from_syscall);

    while (1);

}

static inline void signal_restore_code_put(unsigned char *user_code) {

    /* mov $__NR_sigreturn, %eax */
    *user_code++ = 0xb8;
    *(unsigned long *)user_code = __NR_sigreturn;
    user_code += 4;

    /* int $0x80 */
    *user_code++ = 0xcd;
    *user_code++ = 0x80;

}

int arch_process_execute(struct process *proc, unsigned int eip) {

    unsigned int *kernel_stack, *sighan_stack, flags;
    unsigned char *user_code;

    irq_save(flags);

    user_code = (unsigned char *)proc->mm.start;
    kernel_stack = (unsigned int *)proc->context.esp - 256; /* TODO: */
    sighan_stack = stack_create(PAGE_SIZE);

    proc->signals->context.esp = proc->context.esp;
    proc->signals->context.esp0 = proc->context.esp0;
    proc->signals->context.eip = proc->context.eip;

    proc->signals->context.eax = (unsigned int)sighan_stack;

    push(process_current->pid, sighan_stack);
    push(user_code, sighan_stack);

    exec_kernel_stack_frame(&kernel_stack, (unsigned int)sighan_stack, eip);

    proc->context.eip = (unsigned int)ret_from_syscall;
    proc->context.esp = (unsigned int)kernel_stack;
    proc->context.esp0 = proc->context.esp;

    signal_restore_code_put(user_code);

    irq_restore(flags);

    if (proc == process_current) {
        asm volatile(
                "movl $1f, %0;"
                "movl %%esp, %1;"
                "movl %2, %%esp;"
                "jmp *%3;"
                "1:"
                :: "m" (proc->signals->context.eip),
                   "m" (proc->signals->context.esp),
                   "r" (kernel_stack),
                   "r" (ret_from_syscall)
                : "memory"
        );
    }

    return 0;

}

__noreturn int sys_sigreturn() {

    page_free((void *)(process_current->signals->context.eax - PAGE_SIZE));

    process_current->context.esp0 = process_current->signals->context.esp0;
    set_context(process_current->signals->context.esp,
            process_current->signals->context.eip);

    while (1);

}

