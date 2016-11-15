#include <kernel/kernel.h>
#include <arch/process.h>
#include <kernel/process.h>

unsigned int ram_get() {

    return 256*1024*1024;

}

int arch_kthread_regs_init(struct pt_regs *regs, unsigned int ip) {

    (void)regs; (void)ip;

    return 0;

}

int process_copy(struct process *dest, struct process *src, unsigned int *new_stack, struct pt_regs *regs) {

    (void)dest; (void)src; (void)new_stack; (void)regs;

    return 0;

}

void arch_process_free(struct process *proc) {

    (void)proc;

}

void regs_print(struct pt_regs *regs) {

    (void)regs;

}

void irq_enable(unsigned int irq) {

    (void)irq;

}

void switch_to_user() {

}

void reboot() {}
void shutdown() {}
void copy_from_user(void *dest, void *src, unsigned int size) {

    (void)dest; (void)src; (void)size;

}

void copy_to_user(void *dest, void *src, unsigned int size) {

    (void)dest; (void)src; (void)size;

}

void get_from_user(void *dest, void *src) {
    (void)dest; (void)src;
}

void put_to_user(void *dest, void *src) {
    (void)dest; (void)src;
}

int arch_info_get(char *buffer) {

    (void)buffer;
    return 0;

}

int irqs_list_get(char *buffer) {

    (void)buffer;
    return 0;

}

void swi() {

    printk("hello swi!\n");

}
