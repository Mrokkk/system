#ifndef __OPCODE_H_
#define __OPCODE_H_

struct opcode_decoded_struct {
    unsigned char bytes[24];
    unsigned char size;
    char string[48];
};

struct opcode_struct {
    unsigned char ops[8][3];
    unsigned char size;
    char *name[8];
    unsigned char reversed;
};

#define OP_REG8     1
#define OP_REG16    2
#define OP_REG32    3
#define OP_REGXMM   4
#define OP_SREG     5
#define OP_IMM8     6
#define OP_IMM16    7
#define OP_IMM32    8
#define OP_EAX      9
#define OP_ECX      10
#define OP_EDX      11
#define OP_EBX      12
#define OP_ESP      13
#define OP_EBP      14
#define OP_ESI      15
#define OP_EDI      16
#define OP_AL       17
#define OP_CL       18
#define OP_DL       19
#define OP_BL       20
#define OP_AH       21
#define OP_CH       22
#define OP_DH       23
#define OP_BH       24
#define OP_REL8     25
#define OP_REL16    26
#define OP_REL32    27
#define OP_CREG     28

char *opcode_decode(unsigned char *address, struct opcode_decoded_struct *decoded);

#endif /* __OPCODE_H_ */
