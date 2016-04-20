#ifndef __PROCESSOR_H_
#define __PROCESSOR_H_

#ifndef __ASSEMBLER__

struct context {
    unsigned int dummy;
} __attribute__ ((packed));

struct pt_regs {
    unsigned int dummy;
} __attribute__ ((packed));

extern unsigned int stack_top;
extern int init();
void regs_print(struct pt_regs *regs);

#endif /* __ASSEMBLER__ */

#define INIT_PROCESS_STACK_SIZE 256

#define INIT_PROCESS_STACK

#define INIT_PROCESS_CONTEXT

#endif /* __PROCESSOR_H_ */
