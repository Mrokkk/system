#ifndef __X86_PROCESS_H_
#define __X86_PROCESS_H_

#include <kernel/process.h>

int process_copy(struct process *dest, struct process *src, unsigned int *new_stack, struct pt_regs *regs);
void arch_process_free(struct process *proc);
int arch_kthread_regs_init(struct pt_regs *regs, unsigned int ip);

#endif /* __X86_PROCESS_H_ */
