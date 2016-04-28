#ifndef __X86_PROCESSOR_H_
#define __X86_PROCESSOR_H_

#ifndef __ASSEMBLER__

struct context {
   unsigned long prev_tss;
   unsigned long esp0;
   unsigned long ss0;
   unsigned long esp1;
   unsigned long ss1;
   unsigned long esp2;
   unsigned long ss2;
   unsigned long cr3;
   unsigned long eip;
   unsigned long eflags;
   unsigned long eax;
   unsigned long ecx;
   unsigned long edx;
   unsigned long ebx;
   unsigned long esp;
   unsigned long ebp;
   unsigned long esi;
   unsigned long edi;
   unsigned long es;
   unsigned long cs;
   unsigned long ss;
   unsigned long ds;
   unsigned long fs;
   unsigned long gs;
   unsigned long ldt;
   unsigned short trap;
   unsigned short iomap_offset;
   unsigned char io_bitmap[128];
} __attribute__ ((packed));

struct pt_regs {
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    unsigned int esi;
    unsigned int edi;
    unsigned int ebp;
    unsigned int eax;
    unsigned short ds, __ds;
    unsigned short es, __es;
    unsigned short fs, __fs;
    unsigned short gs, __gs;
    unsigned int eip;
    unsigned short cs, __cs;
    unsigned int eflags;
    unsigned int esp;
    unsigned short ss, __ss;
} __attribute__ ((packed));

extern void ret_from_process();
extern int init();
void regs_print(struct pt_regs *regs);

#endif /* __ASSEMBLER__ */

#define REGS_EBX    0
#define REGS_ECX    4
#define REGS_EDX    8
#define REGS_ESI    12
#define REGS_EDI    16
#define REGS_EBP    20
#define REGS_EAX    24
#define REGS_DS     28
#define REGS_ES     32
#define REGS_FS     36
#define REGS_GS     40
#define REGS_EIP    44
#define REGS_CS     48
#define REGS_EFLAGS 52
#define REGS_ESP    56
#define REGS_SS     60

#define INIT_PROCESS_STACK_SIZE 256

#define INIT_PROCESS_STACK

#define INIT_PROCESS_CONTEXT(name) \
        .iomap_offset = 104, \
        .esp0 = (unsigned long)&name##_stack[INIT_PROCESS_STACK_SIZE], \
        .esp = (unsigned long)&name##_stack[INIT_PROCESS_STACK_SIZE], \
        .ss0 = 0x10,

#endif /* __X86_PROCESSOR_H_ */
