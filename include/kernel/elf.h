#ifndef __ELF_H_
#define __ELF_H_

#define ELF_NIDENT    16

struct elf32_header {
    unsigned char    e_ident[ELF_NIDENT];
    unsigned short    e_type;
    unsigned short    e_machine;
    unsigned int    e_version;
    unsigned int    e_entry;
    unsigned int    e_phoff;
    unsigned int    e_shoff;
    unsigned int    e_flags;
    unsigned short    e_ehsize;
    unsigned short    e_phentsize;
    unsigned short    e_phnum;
    unsigned short    e_shentsize;
    unsigned short    e_shnum;
    unsigned short    e_shstrndx;
};

enum _e_ident {
    EI_MAG0         = 0, /* 0x7F */
    EI_MAG1         = 1, /* 'E' */
    EI_MAG2         = 2, /* 'L' */
    EI_MAG3         = 3, /* 'F' */
    EI_CLASS        = 4, /* Architecture (32/64) */
    EI_DATA         = 5, /* Byte Order */
    EI_VERSION      = 6, /* ELF Version */
    EI_OSABI        = 7, /* OS Specific */
    EI_ABIVERSION   = 8, /* OS Specific */
    EI_PAD          = 9  /* Padding */
};

#define ELFMAG0    0x7F /* e_ident[EI_MAG0] */
#define ELFMAG1    'E'  /* e_ident[EI_MAG1] */
#define ELFMAG2    'L'  /* e_ident[EI_MAG2] */
#define ELFMAG3    'F'  /* e_ident[EI_MAG3] */

#define ELFDATA2LSB     (1)  /* Little Endian */
#define ELFCLASS32      (1)  /* 32-bit Architecture */

enum _e_type {
    ET_NONE = 0, /* Unkown Type */
    ET_REL  = 1, /* Relocatable File */
    ET_EXEC = 2  /* Executable File */
};

#define EM_386        (3)  /* x86 Machine Type */
#define EV_CURRENT    (1)  /* ELF Current Version */

struct elf32_segment_header {
    unsigned int    sh_name;
    unsigned int    sh_type;
    unsigned int    sh_flags;
    unsigned int    sh_addr;
    unsigned int    sh_offset;
    unsigned int    sh_size;
    unsigned int    sh_link;
    unsigned int    sh_info;
    unsigned int    sh_addralign;
    unsigned int    sh_entsize;
};

#define SHN_UNDEF    (0x00) /* Undefined/Not present */

enum _sh_type {
    SHT_NULL        = 0,   /* Null section */
    SHT_PROGBITS    = 1,   /* Program information */
    SHT_SYMTAB      = 2,   /* Symbol table */
    SHT_STRTAB      = 3,   /* String table */
    SHT_RELA        = 4,   /* Relocation (w/ addend) */
    SHT_NOBITS      = 8,   /* Not present in file */
    SHT_REL         = 9   /* Relocation (no addend) */
};

enum _sh_flags {
    SHF_WRITE    = 0x01, /* Writable section */
    SHF_ALLOC    = 0x02  /* Exists in memory */
};

int elf_check_file(struct elf32_header *hdr);

#endif /* __ELF_H_ */
