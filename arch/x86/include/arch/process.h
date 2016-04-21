#ifndef __X86_PROCESS_H_
#define __X86_PROCESS_H_

struct process;
struct pt_regs;

int arch_process_copy(struct process *dest, struct process *src, struct pt_regs *old_regs);
void arch_process_free(struct process *proc);
int arch_kprocess_regs_init(struct pt_regs *regs, unsigned int ip);
int arch_exec(struct pt_regs *regs);
int arch_process_init(struct process *proc);
void regs_print(struct pt_regs *regs);

#endif /* __X86_PROCESS_H_ */
