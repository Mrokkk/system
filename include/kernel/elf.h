#pragma once

#include <stdint.h>
#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/binary.h>

#define ELF_NIDENT    16

enum e_type
{
    ET_NONE     = 0, // Unkown Type
    ET_REL      = 1, // Relocatable File
    ET_EXEC     = 2, // Executable File
};

enum e_ident
{
    EI_MAG0     = 0, // 0x7F
    EI_MAG1     = 1, // 'E'
    EI_MAG2     = 2, // 'L'
    EI_MAG3     = 3, // 'F'
    EI_CLASS    = 4, // Architecture (32/64)
    EI_DATA     = 5, // Byte Order
    EI_VERSION  = 6, // ELF Version
    EI_OSABI    = 7, // OS Specific
    EI_ABIVERSION = 8, // OS Specific
};

enum e_ident_mag
{
    ELFMAG0     = 0x7F, // e_ident[EI_MAG0]
    ELFMAG1     = 'E', // e_ident[EI_MAG1]
    ELFMAG2     = 'L', // e_ident[EI_MAG2]
    ELFMAG3     = 'F', // e_ident[EI_MAG3]
};

enum e_ident_class
{
    ELFCLASS32  = 1, // 32-bit Architecture
    ELFCLASS64  = 2, // 64-bit Architecture
};

enum e_ident_data
{
    ELFDATA2LSB = 1, // Little Endian
    ELFDATA2MSB = 2, // Big Endian
};

enum e_machine
{
    EM_386      = 3, // x86 Machine Type
};

enum e_version
{
    EV_CURRENT  = 1, // ELF Current Version
};

typedef struct elf32_header
{
    uint8_t     e_ident[ELF_NIDENT];
    uint16_t    e_type;
    uint16_t    e_machine;
    uint32_t    e_version;
    uint32_t    e_entry;
    uint32_t    e_phoff;
    uint32_t    e_shoff;
    uint32_t    e_flags;
    uint16_t    e_ehsize;
    uint16_t    e_phentsize;
    uint16_t    e_phnum;
    uint16_t    e_shentsize;
    uint16_t    e_shnum;
    uint16_t    e_shstrndx;
} elf32_header_t;

typedef struct elf32_phdr{
    uint32_t    p_type;
#define PT_NULL     0
#define PT_LOAD     1
#define PT_DYNAMIC  2
#define PT_INTERP   3
#define PT_NOTE     4
#define PT_SHLIB    5
#define PT_PHDR     6
#define PT_LOPROC   0x70000000
#define PT_HIPROC   0x7fffffff

    uint32_t    p_offset;
    uint32_t    p_vaddr;
    uint32_t    p_paddr;
    uint32_t    p_filesz;
    uint32_t    p_memsz;
    uint32_t    p_flags;
#define PF_R        0x4
#define PF_W        0x2
#define PF_X        0x1

    uint32_t    p_align;
} elf32_phdr_t;

typedef struct elf32_segment_header
{
    uint32_t sh_name;

    uint32_t sh_type;
#define SHT_NULL        0 // Null section
#define SHT_PROGBITS    1 // Program information
#define SHT_SYMTAB      2 // Symbol table
#define SHT_STRTAB      3 // String table
#define SHT_RELA        4 // Relocation (w/ addend)
#define SHT_NOBITS      8 // Not present in file
#define SHT_REL         9 // Relocation (no addend)

    uint32_t sh_flags;
#define SHF_WRITE               0x01 // Writable section
#define SHF_ALLOC               0x02 // Exists in memory
#define SHF_EXECINSTR           0x04 // Executable machine instructions
#define SHF_MERGE               0x10 // Data may be merged
#define SHF_STRINGS             0x20 // Strings
#define SHF_INFO_LINK           0x40 // Section header table index
#define SHF_OS_NONCONFORMING    0x100 // OS-specific
#define SHF_GROUP               0x200 // Member of a group
#define SHF_TLS                 0x400 // Thread-Local Storage
#define SHF_MASKOS              0x0ff00000 // OS-specific
#define SHF_MASKPROC            0xf0000000 // Processor-specific

    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} elf32_shdr_t;

typedef struct
{
    uint32_t    st_name;
    uint32_t    st_value;
    uint32_t    st_size;
    uint8_t     st_info;
    uint8_t     st_other;
    uint16_t    st_shndx;
} elf32_sym;

int elf_check_file(struct elf32_header* hdr);
int elf_load(
    const char* name,
    file_t* file,
    binary_t* bin);
