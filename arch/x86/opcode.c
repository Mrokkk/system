#include <arch/opcode.h>
#include <kernel/kernel.h>
#include <kernel/string.h>

struct opcode_struct opcodes_1[] = {
    [0x00] = {{{OP_REG8, OP_REG8}}, 2, {"add"}, 0},
    [0x01] = {{{OP_REG32, OP_REG32}}, 2, {"add"}, 0},
    [0x02] = {{{OP_REG8, OP_REG8}}, 2, {"add"}, 0},
    [0x03] = {{{OP_REG32, OP_REG32}}, 2, {"add"}, 1},
    [0x04] = {{{OP_AL, OP_IMM8}}, 2, {"add"}, 0},
    [0x05] = {{{OP_EAX, OP_IMM32}}, 5, {"add"}, 0},
    [0x06] = {{{0, }}, 1, {"push"}, 0},
    [0x07] = {{{0, }}, 1, {"pop"}, 0},
    [0x08] = {{{OP_REG8, OP_REG8}}, 1, {"or"}, 0},
    [0x09] = {{{OP_REG32, OP_REG32}}, 2, {"or"}, 0},
    [0x0a] = {{{OP_REG8, OP_REG8}}, 2, {"or"}, 1},
    [0x0b] = {{{OP_REG32, OP_REG32}}, 2, {"or"}, 1},
    [0x0c] = {{{OP_AL, OP_IMM8}}, 3, {"or"}, 1},
    [0x11] = {{{OP_REG32, OP_REG32}}, 2, {"adc"}, 0},
    [0x18] = {{{OP_REG8, OP_REG8}}, 2, {"sbb"}, 0},
    [0x19] = {{{OP_REG32, OP_REG32}}, 2, {"sbb"}, 0},
    [0x20] = {{{OP_REG8, OP_REG8}}, 2, {"and"}, 0},
    [0x21] = {{{OP_REG32, OP_REG32}}, 2, {"and"}, 0},
    [0x22] = {{{OP_REG8, OP_REG8}}, 2, {"and"}, 0},
    [0x25] = {{{OP_EAX, OP_IMM32}}, 5, {"and"}, 0},
    [0x29] = {{{OP_REG32, OP_REG32}}, 2,  {"sub"}, 0},
    [0x2b] = {{{OP_REG32, OP_REG32}}, 2,  {"sub"}, 0},
    [0x30] = {{{OP_REG8, OP_REG8}}, 2, {"xor"}, 0},
    [0x31] = {{{OP_REG32, OP_REG32}}, 2, {"xor"}, 0},
    [0x38] = {{{OP_REG8, OP_REG8}}, 2, {"cmp"}, 0},
    [0x39] = {{{OP_REG32, OP_REG32}}, 2, {"cmp"}, 0},
    [0x3a] = {{{OP_REG8, OP_REG8}}, 2, {"cmp"}, 0},
    [0x3b] = {{{OP_REG32, OP_REG32}}, 2, {"cmp"}, 0},
    [0x3c] = {{{OP_AL, OP_IMM8}}, 2, {"cmp"}, 0},
    [0x3d] = {{{OP_EAX, OP_IMM32}}, 5, {"cmp"}, 0},
    [0x40] = {{{OP_EAX}}, 1, {"inc"}, 0},
    [0x41] = {{{OP_ECX}}, 1, {"inc"}, 0},
    [0x42] = {{{OP_EDX}}, 1, {"inc"}, 0},
    [0x43] = {{{OP_EBX}}, 1, {"inc"}, 0},
    [0x44] = {{{OP_ESP}}, 1, {"inc"}, 0},
    [0x45] = {{{OP_EBP}}, 1, {"inc"}, 0},
    [0x46] = {{{OP_ESI}}, 1, {"inc"}, 0},
    [0x47] = {{{OP_EDI}}, 1, {"inc"}, 0},
    [0x48] = {{{OP_EAX}}, 1, {"dec"}, 0},
    [0x49] = {{{OP_ECX}}, 1, {"dec"}, 0},
    [0x4a] = {{{OP_EDX}}, 1, {"dec"}, 0},
    [0x4b] = {{{OP_EBX}}, 1, {"dec"}, 0},
    [0x4c] = {{{OP_ESP}}, 1, {"dec"}, 0},
    [0x4d] = {{{OP_EBP}}, 1, {"dec"}, 0},
    [0x4e] = {{{OP_ESI}}, 1, {"dec"}, 0},
    [0x4f] = {{{OP_EDI}}, 1, {"dec"}, 0},
    [0x50] = {{{OP_EAX, 0}}, 1, {"push"}, 0},
    [0x51] = {{{OP_ECX, 0}}, 1, {"push"}, 0},
    [0x52] = {{{OP_EDX, 0}}, 1, {"push"}, 0},
    [0x53] = {{{OP_EBX, 0}}, 1, {"push"}, 0},
    [0x54] = {{{OP_ESP, 0}}, 1, {"push"}, 0},
    [0x55] = {{{OP_EBP, 0}}, 1, {"push"}, 0},
    [0x56] = {{{OP_ESI, 0}}, 1, {"push"}, 0},
    [0x57] = {{{OP_EDI, 0}}, 1, {"push"}, 0},
    [0x58] = {{{OP_EAX, 0}}, 1, {"pop"}, 0},
    [0x59] = {{{OP_ECX, 0}}, 1, {"pop"}, 0},
    [0x5a] = {{{OP_EDX, 0}}, 1, {"pop"}, 0},
    [0x5b] = {{{OP_EBX, 0}}, 1, {"pop"}, 0},
    [0x5c] = {{{OP_ESP, 0}}, 1, {"pop"}, 0},
    [0x5d] = {{{OP_EBP, 0}}, 1, {"pop"}, 0},
    [0x5e] = {{{OP_ESI, 0}}, 1, {"pop"}, 0},
    [0x5f] = {{{OP_EDI, 0}}, 1, {"pop"}, 0},
    [0x60] = {{{0, }}, 1, {"pusha"}, 0},
    [0x61] = {{{0, }}, 1, {"popa"}, 0},
    [0x68] = {{{OP_IMM32, 0}}, 5, {"push"}, 0},
    [0x6a] = {{{OP_IMM8, 0}}, 2, {"push"}, 0},
    [0x6b] = {{{OP_REG32, OP_REG32, OP_IMM8}}, 3, {"imul"}, 0},
    [0x72] = {{{OP_REL8, 0}}, 2, {"jb"}, 0},
    [0x74] = {{{OP_REL8, 0}}, 2, {"je"}, 0},
    [0x73] = {{{OP_REL8, 0}}, 2, {"jae"}, 0},
    [0x75] = {{{OP_REL8, 0}}, 2, {"jne"}, 0},
    [0x76] = {{{OP_REL8, 0}}, 2, {"jbe"}, 0},
    [0x77] = {{{OP_REL8, 0}}, 2, {"ja"}, 0},
    [0x78] = {{{OP_REL8, 0}}, 2, {"js"}, 0},
    [0x79] = {{{OP_REL8, 0}}, 2, {"jns"}, 0},
    [0x7c] = {{{OP_REL8, }}, 2, {"jl"}, 0},
    [0x7d] = {{{OP_REL8, }}, 2, {"jge"}, 0},
    [0x7e] = {{{OP_REL8, }}, 2, {"jle"}, 0},
    [0x7f] = {{{OP_REL8, }}, 2, {"jg"}, 0},
    [0x80] = {{{OP_REG8, OP_IMM8}}, 3, {"add", "or", "adc", "sbb", "and", "sub", "xor", "cmp"}, 0},
    [0x81] = {{{OP_REG32, OP_IMM32}}, 6, {"add", "or", "adc", "sbb", "and", "sub", "xor", "cmp"}, 0},
    [0x83] = {{{OP_REG32, OP_IMM8}}, 3, {"add", "or", "adc", "sbb", "and", "sub", "xor", "cmp"}, 0},
    [0x84] = {{{OP_REG8, OP_REG8}}, 2, {"test"}, 0},
    [0x85] = {{{OP_REG32, OP_REG32}}, 2, {"test"}, 0},
    [0x87] = {{{OP_REG32, OP_REG32}}, 2, {"xchg"}, 0},
    [0x88] = {{{OP_REG8, OP_REG8}}, 2, {"mov"}, 0}, /* Some problem */
    [0x89] = {{{OP_REG32, OP_REG32}}, 2, {"mov"}, 0},
    [0x8a] = {{{OP_REG8, OP_REG8}}, 2, {"mov"}, 1},
    [0x8b] = {{{OP_REG32, OP_REG32}}, 2, {"mov"}, 1},
    [0x8c] = {{{OP_REG16, OP_SREG}}, 2, {"mov"}, 0},
    [0x8d] = {{{OP_REG32, OP_REG32}}, 2, {"lea"}, 1}, /* TODO: Should be other operand */
    [0x8e] = {{{OP_REG16, OP_SREG}}, 2, {"mov"}, 1},
    [0x90] = {{{0, }}, 1, {"nop"}, 0},
    [0x98] = {{{0, }}, 1, {"cwtl"}, 0},
    [0x9c] = {{{0, }}, 1, {"pushf"}, 0},
    [0x9d] = {{{0, }}, 1, {"popf"}, 0},
    [0xa1] = {{{OP_EAX, OP_IMM32}}, 5, {"mov"}, 0},
    [0xa2] = {{{OP_IMM32, OP_AL}}, 5, {"mov"}, 0}, /* Should be moffs8 */
    [0xa3] = {{{OP_REL32, OP_EAX}}, 5, {"mov"}, 0},
    [0xa4] = {{{OP_EDI, OP_ESI}}, 1, {"movsb"}, 0}, /* TODO */
    [0xa5] = {{{OP_EDI, OP_ESI}}, 1, {"movsw"}, 0}, /* TODO */
    [0xa8] = {{{OP_AL, OP_IMM8}}, 1, {"test"}, 0}, /* TODO */
    [0xa9] = {{{OP_EAX, OP_IMM32}}, 1, {"test"}, 0}, /* TODO */
    [0xb0] = {{{OP_AL, OP_IMM8}}, 2, {"mov"}, 0},
    [0xb1] = {{{OP_CL, OP_IMM8}}, 2, {"mov"}, 0},
    [0xb2] = {{{OP_DL, OP_IMM8}}, 2, {"mov"}, 0},
    [0xb3] = {{{OP_BL, OP_IMM8}}, 2, {"mov"}, 0},
    [0xb8] = {{{OP_EAX, OP_IMM32}}, 5, {"mov"}, 0},
    [0xb9] = {{{OP_ECX, OP_IMM32}}, 5, {"mov"}, 0},
    [0xba] = {{{OP_EDX, OP_IMM32}}, 5, {"mov"}, 0},
    [0xbb] = {{{OP_EBX, OP_IMM32}}, 5, {"mov"}, 0},
    [0xbc] = {{{OP_ESP, OP_IMM32}}, 5, {"mov"}, 0},
    [0xbd] = {{{OP_EBP, OP_IMM32}}, 5, {"mov"}, 0},
    [0xbe] = {{{OP_ESI, OP_IMM32}}, 5, {"mov"}, 0},
    [0xbf] = {{{OP_EDI, OP_IMM32}}, 5, {"mov"}, 0},
    [0xc0] = {{{OP_REG8, OP_IMM8}}, 3, {"rol", "ror", "rcl", "rcr", "shl", "shr", "shl", "sal"}, 0},
    [0xc1] = {{{OP_REG32, OP_IMM8}}, 3, {"rol", "ror", "rcl", "rcr", "shl", "shr", "shl", "sal"}, 0},
    [0xc3] = {{{}}, 1, {"ret"}, 0},
    [0xc6] = {{{OP_REG8, OP_IMM8}}, 3, {"mov"}, 0},   /* Some problem */
    [0xc7] = {{{OP_REG32, OP_IMM32}}, 6, {"mov"}, 0}, /* Some problem */
    [0xc9] = {{{0, }}, 1, {"leave"}, 0},
    [0xcd] = {{{OP_IMM8, 0}}, 2, {"int"}, 0},
    [0xcf] = {{{0}}, 1, {"iret"}, 0},
    [0xd0] = {{{OP_REG8}}, 2, {"shl"}, 0},    /* TODO */
    [0xd1] = {{{OP_REG32}, {OP_REG32}, {OP_REG32}, {OP_REG32}, {OP_REG32}, {OP_REG32}, {OP_REG32}, {OP_REG32}}, 2, {"rol", "ror", "rcl", "rcr", "shl", "shr", "shl", "sar"}, 0},    /* TODO */
    [0xe6] = {{{OP_IMM8, OP_AL}}, 2, {"out"}, 0},
    [0xe8] = {{{OP_REL32, 0}}, 5, {"call"}, 0},
    [0xe9] = {{{OP_REL32, 0}}, 5, {"jmp"}, 0},
    [0xea] = {{{OP_IMM16, OP_IMM32}}, 7, {"ljmp"}, 1},
    [0xeb] = {{{OP_REL8, 0}}, 2, {"jmp"}, 0},
    [0xec] = {{{OP_AL, OP_EDX}}, 1, {"in"}, 0},
    [0xee] = {{{OP_EDX, OP_AL}}, 1, {"out"}, 0},
    [0xf3] = {{{0}}, 1, {"rep"}, 0},
    [0xf4] = {{{0}}, 1, {"hlt"}, 0},
    [0xf6] = {{{OP_REG8, OP_IMM8}, {OP_REG8, OP_IMM8}, {OP_REG8}, {OP_REG8}, {OP_REG8}, {OP_REG8}, {OP_REG8}, {OP_REG8}}, 2, {"test", "test", "not", "neg", "mul", "imul", "div", "idiv"}, 0}, /* TODO */
    [0xf7] = {{{OP_REG32, OP_IMM32}, {OP_REG32, OP_IMM32}, {OP_REG32}, {OP_REG32}, {OP_REG32}, {OP_REG32}, {OP_REG32}, {OP_REG32}}, 2, {"test", "test", "not", "neg", "mul", "imul", "div", "idiv"}, 0}, /* TODO */
    [0xfa] = {{{0}}, 1, {"cli"}, 0},
    [0xfb] = {{{0}}, 1, {"sti"}, 0},
    [0xfc] = {{{0}}, 1, {"cld"}, 0},
    [0xfd] = {{{0}}, 1, {"std"}, 0},
    [0xfe] = {{{OP_REG8}}, 1, {"inc", "dec"}, 0},
    [0xff] = {{{OP_REG32, 0}}, 2, {"inc", "dec", "call", "callf", "jmp", "jumpf", "push"}, 0}
};


struct opcode_struct opcodes_2[] = {
    [0x00] = {{{OP_REG32, }}, 2, {"sldt", "str", "lldt", "ltr", "verr", "verw"}, 0},
    [0x01] = {{{OP_IMM32, 0}}, 6, {"lgdt"}, 0},
    [0x10] = {{{OP_REGXMM, OP_REGXMM}}, 2, {"movups"}, 1},
    [0x18] = {{{OP_REG32, 0}}, 0, {"prefetchnta"}, 0},
    [0x20] = {{{OP_REG32, OP_CREG}}, 2, {"mov"}, 0},
    [0x28] = {{{OP_REGXMM, OP_REGXMM}}, 2, {"movaps"}, 1},
    [0x2b] = {{{OP_REGXMM, OP_REGXMM}}, 2, {"movntps"}, 1},
    [0x40] = {{{OP_REG32, OP_REG32}}, 0, {"cmovo"}, 1},
    [0x41] = {{{OP_REG32, OP_REG32}}, 0, {"cmovno"}, 1},
    [0x42] = {{{OP_REG32, OP_REG32}}, 0, {"cmovb"}, 1},
    [0x43] = {{{OP_REG32, OP_REG32}}, 0, {"cmovnb"}, 1},
    [0x44] = {{{OP_REG32, OP_REG32}}, 0, {"cmove"}, 1},
    [0x45] = {{{OP_REG32, OP_REG32}}, 0, {"cmovne"}, 1},
    [0x46] = {{{OP_REG32, OP_REG32}}, 0, {"cmovna"}, 1},
    [0x47] = {{{OP_REG32, OP_REG32}}, 0, {"cmova"}, 1},
    [0x48] = {{{OP_REG32, OP_REG32}}, 0, {"cmovs"}, 1},
    [0x49] = {{{OP_REG32, OP_REG32}}, 0, {"cmovns"}, 1},
    [0x4a] = {{{OP_REG32, OP_REG32}}, 0, {"cmovp"}, 1},
    [0x4b] = {{{OP_REG32, OP_REG32}}, 0, {"cmovnp"}, 1},
    [0x4c] = {{{OP_REG32, OP_REG32}}, 0, {"cmovnge"}, 1},
    [0x4d] = {{{OP_REG32, OP_REG32}}, 0, {"cmovge"}, 1},
    [0x4e] = {{{OP_REG32, OP_REG32}}, 0, {"cmovle"}, 1},
    [0x4f] = {{{OP_REG32, OP_REG32}}, 0, {"cmovnle"}, 1},
    [0x77] = {{{}}, 1, {"emms"}, 0},
    [0x82] = {{{OP_REL32, }}, 5, {"jb"}, 0},
    [0x83] = {{{OP_REL32, }}, 5, {"jae"}, 0},
    [0x84] = {{{OP_REL32, }}, 5, {"je"}, 0},
    [0x85] = {{{OP_REL32, }}, 5, {"jne"}, 0},
    [0x86] = {{{OP_REL32, }}, 5, {"jbe"}, 0},
    [0x87] = {{{OP_REL32, }}, 5, {"ja"}, 0},
    [0x88] = {{{OP_REL32, }}, 5, {"js"}, 0},
    [0x8c] = {{{OP_REL32, }}, 5, {"jl"}, 0},
    [0x8e] = {{{OP_REL32, }}, 5, {"jle"}, 0},
    [0x8f] = {{{OP_REL32, }}, 5, {"jge"}, 0},
    [0x94] = {{{OP_REG8}}, 2, {"sete"}, 0},
    [0x95] = {{{OP_REG8}}, 2, {"setne"}, 0},
    [0x96] = {{{OP_REG8}}, 2, {"setbe"}, 0},
    [0x9e] = {{{OP_IMM8}}, 2, {"setle"}, 0},
    [0xa2] = {{{}}, 1, {"cpuid"}, 0},
    [0xae] = {{{OP_REG32}, {}, {}, {}, {}, {}, {}, {OP_REG8}}, 1, {"fxsave", "fxrstor", "ldmxcsr", "xsave", "lfence", "xrstor", "mfence", "sfence"}, 0},
    [0xaf] = {{{OP_REG32, OP_REG32}}, 2, {"imul"}, 1},
    [0xb6] = {{{OP_REG8, OP_REG32}}, 2, {"movzx"}, 1}, /* Some problem with order */
    [0xb7] = {{{OP_REG32, OP_REG32}}, 2, {"movzx"}, 1},
    [0xbe] = {{{OP_REG32, OP_REG8}}, 2, {"movsx"}, 1},
    [0xbf] = {{{OP_REG32, OP_REG16}}, 2, {"movsx"}, 1},
    [0xff] = {{{}}, 0, {"nnn"}, 0}
};

char *(regs[5][8]) = {
    {"", },
    {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"},
    {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"},
    {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"},
    {"xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"},
};

char *sreg[] = {
    "es",
    "cs",
    "ss",
    "ds",
    "fs",
    "gs",
    "res",
    "res"
};

char *creg[] = {
    "cr0",
    "cr1",
    "cr2",
    "cr3"
};

/*===========================================================================*
 *                              opcode_decode                                *
 *===========================================================================*/
char *opcode_decode(unsigned char *address, struct opcode_decoded_struct *decoded) {

    char operand[3][32];
    int operands = 0, registers = 0, sregisters = 0;
    unsigned int i, op;
    char size_prefix = 0;
    struct opcode_struct *opcodes_table;
    unsigned char mod = 255;
    unsigned char sib = 0;
    unsigned char o = 255, o2 = 0;
    unsigned char offset = 0;
    unsigned char mod_rm = 0;
    int size_add = 0, size = 0;
    unsigned char opcode;
    unsigned char imm_offset = 0;
    unsigned char register_type = 0;
    unsigned char address_offset = 0;
    unsigned char have_sib = 0, have_modrm = 0;
    unsigned char ss = 0;
    unsigned char mult = 1;     /* Multiplier */

    size = 0;

    if (address[0] == 0x66) {    /* Check if first byte is 16-bit prefix */
        address = address + 1;
        address_offset++;
        size_prefix = 1;
        size_add++;
    }

    /* If first byte is 0xf, instruction is 2 bytes long */
    if (address[0] == 0x0f) {
        opcodes_table = opcodes_2;
        address = address + 1;
        address_offset++;
        size_add++;
        size++;
    } else {
        opcodes_table = opcodes_1;
        size++;
    }

    opcode = address[0];
    mod_rm = address[1];
    sib = address[2];

    if (opcodes_table[opcode].name[0] == 0) {
        decoded->size = 0;
        return 0;
    }

    for (i=0; i<3; i++) {
        if (opcodes_table[opcode].ops[0][i] == OP_REG32 || opcodes_table[opcode].ops[0][i] == OP_REG16
                || opcodes_table[opcode].ops[0][i] == OP_REG8 || opcodes_table[opcode].ops[0][i] == OP_REGXMM ) {
            mod = (address[1] >> 6) & 0x3;
            registers++;
        } else if (opcodes_table[opcode].ops[0][i] == OP_SREG)
            sregisters++;
    }

    if (registers + sregisters == 1) {
        o = (mod_rm >> 3) & 0x7;
    }

    if (registers + sregisters > 0) {
        have_modrm = 1;
        size++;
    }

    if (have_modrm) {
        imm_offset++;
        mod = (mod_rm >> 6) & 3;
        switch (mod) { /* Check offsets */
            case 0:
                if ((mod_rm & 7) == 5) { /* We've got imm32 */
                    size_add += 4;
                    imm_offset += 4;
                }
                break;
            case 1:    /* We've got disp8 */
                size_add++;
                imm_offset++;
                offset++;
                break;
            case 2: /* We've got disp32 */
                size_add += 4;
                imm_offset += 4;
                break;
            case 3: /* We've got nothing */
                break;
        }
        if (((mod_rm & 7) == 4) && (mod != 3)) { /* Check for SIB */
            have_sib = 1;    /* In SIB byte order is reversed!!! */
            imm_offset++;
            size_add++;
            ss = (sib >> 6) & 3;
            if ((sib & 7) == 5) {
                if (mod == 0)
                    size_add += 4;
                else if (mod == 2)
                    size_add += 4;
            }
            mult = 1 << ss;
        }
    }

    op = mod_rm;

    if (o == 255) o2 = 0;
    else o2 = o;

    if (opcodes_table[opcode].ops[o2][0] == 0) o2 = 0;

    for (i=0; i<3; i++) {
        switch (opcodes_table[opcode].ops[o2][i]) {
            case OP_REG8:
            case OP_REG16:
            case OP_REG32:
            case OP_REGXMM:
                register_type = opcodes_table[opcode].ops[o2][i];
                /*if (i == 0) {*/
                if (mod_rm < 0xc0) {
                    if (size_prefix)
                        register_type = OP_REG16;
                    else register_type = OP_REG32;
                } else {
                    if (register_type == OP_REG32) {
                        if (size_prefix)
                            register_type = OP_REG16;
                        else register_type = OP_REG32;
                    }
                }
                /*}*/
                if (i > 0) {
                    sprintf(operand[i], "%%%s", regs[register_type][op & 7]);
                    operands++;
                    break;
                }
                if (have_sib) {
                    switch (ss) {
                        case 0:
                        case 1:
                        case 2:
                        case 3:
                            if (((sib >> 3) & 7) == 4) { /* There's no index in SIB */
                                if (mod == 0)
                                    sprintf(operand[i], "(%%%s)", regs[register_type][op & 7]);
                                else if (mod == 1)
                                    sprintf(operand[i], "%d(%%%s)", (signed char)address[3], regs[register_type][op & 7]);
                                else if (mod == 2)
                                    sprintf(operand[i], "%d(%%%s)", *((signed int *)&address[3]), regs[register_type][op & 7]);
                            } else { /* There's an index in SIB which is either EAX, EBX, ECX, EDX, EDI, ESI or EBP */
                                if ((sib & 7) == 5) { /* There's also an offset */
                                    if (mod == 0)
                                        sprintf(operand[i], "%d(%d*%%%s)", *((signed int *)&address[3]), mult, regs[3][(sib >> 3) & 7]);
                                    else if (mod == 1)
                                        sprintf(operand[i], "%d(%%%s+%d*%%%s)", (signed char)address[3], regs[3][5], mult, regs[3][(sib >> 3) & 7]);
                                    else if (mod == 2)
                                        sprintf(operand[i], "%d(%%%s+%d*%%%s)", *((signed int *)&address[3]), regs[3][5], mult, regs[3][(sib >> 3) & 7]);
                                }
                                else {
                                    if (mod == 0)
                                        sprintf(operand[i], "(%%%s+%d*%%%s)", regs[register_type][op & 7], mult, regs[3][(sib >> 3) & 7]);
                                    else if (mod == 1)
                                        sprintf(operand[i], "%d(%%%s+%d*%%%s)", (signed char)address[3], regs[register_type][(sib >> 3) & 7], mult, regs[3][(sib >> 3) & 7]);
                                    else if (mod == 2)
                                        sprintf(operand[i], "%d(%%%s+%d*%%%s)", *((signed int *)&address[3]), regs[register_type][(sib >> 3) & 7], mult, regs[3][(sib >> 3) & 7]);
                                }
                            }
                            break;
                    }
                } else {
                    switch (mod) {
                        case 0:
                            if ((mod_rm & 7) == 5)
                                sprintf(operand[i], "0x%x", *((unsigned int *)&address[2]));
                            else
                                sprintf(operand[i], "(%%%s)", regs[register_type][op & 7]);
                            break;
                        case 1:
                            sprintf(operand[i], "%d(%%%s)", (signed char)address[2], regs[register_type][op & 7]);
                            break;
                        case 2:
                            sprintf(operand[i], "%d(%%%s)", *((signed int *)&address[2]), regs[register_type][op & 7]);
                            break;
                        case 3:
                            sprintf(operand[i], "%%%s", regs[register_type][op & 7]);
                            break;
                    }
                }
                operands++;
                break;

            case OP_SREG:
                sprintf(operand[i], "%%%s", sreg[op & 7]);
                operands++;
                break;
            case OP_IMM8:
                sprintf(operand[i], "$0x%x", (unsigned int)address[1+imm_offset] & 0xff);
                operands++;
                size++;
                break;
            case OP_IMM16:
                sprintf(operand[i], "$0x%x", (unsigned int)address[1+imm_offset] & 0xffff);
                operands++;
                size += 2;
                break;
            case OP_IMM32:
                if (size_prefix) {
                    sprintf(operand[i], "$0x%x", *((unsigned short *)&address[1+imm_offset]) & 0xffff);
                    size_add -= 2;
                } else sprintf(operand[i], "$0x%x", *((unsigned int *)&address[1+imm_offset]));
                operands++;
                size += 4;
                break;
            case OP_EAX:
            case OP_EBX:
            case OP_ECX:
            case OP_EDX:
            case OP_ESP:
            case OP_EBP:
            case OP_ESI:
            case OP_EDI:
                if (size_prefix)
                    sprintf(operand[i], "%%%s", regs[2][opcodes_table[opcode].ops[o2][i]-OP_EAX]);
                else sprintf(operand[i], "%%%s", regs[3][opcodes_table[opcode].ops[o2][i]-OP_EAX]);
                operands++;
                break;
            case OP_AL:
            case OP_CL:
            case OP_DL:
            case OP_BL:
            case OP_AH:
            case OP_CH:
            case OP_DH:
            case OP_BH:
                sprintf(operand[i], "%%%s", regs[1][opcodes_table[opcode].ops[o2][i]-OP_AL]);
                operands++;
                break;
            case OP_REL8:
                sprintf(operand[i], ".%+d", (signed char)address[1+imm_offset]);
                operands++;
                size++;
                break;
            case OP_REL16:
                sprintf(operand[i], ".%+d", *((signed short *)&address[1+imm_offset]));
                operands++;
                size += 2;
                break;
            case OP_REL32:
                sprintf(operand[i], ".%+d", *((int *)&address[1+imm_offset]));
                operands++;
                size += 4;
                break;
            case OP_CREG:
                sprintf(operand[i], "%%%s", creg[op & 7]);
                operands++;
                break;

        }
        op = op >> 3;
    }

    if (o == 255) o = 0;

    if (operands == 1) {
        sprintf(decoded->string, "%s %s", opcodes_table[address[0]].name[o], operand[0]);
    } else if (operands == 2) {
        if (opcodes_table[address[0]].reversed)
            sprintf(decoded->string, "%s %s, %s", opcodes_table[address[0]].name[o], operand[0], operand[1]);
        else sprintf(decoded->string, "%s %s, %s", opcodes_table[address[0]].name[o], operand[1], operand[0]);
    } else if (operands == 3) {
        sprintf(decoded->string, "%s %s, %s, %s", opcodes_table[address[0]].name[o], operand[0], operand[1], operand[2]);
    } else
        sprintf(decoded->string, "%s", opcodes_table[address[0]].name[o]);

    decoded->size = size + size_add;

    for (i=0; i<decoded->size; i++)
        decoded->bytes[i] = address[i-address_offset];

    return decoded->string;

}
