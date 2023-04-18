#pragma once

#include <stdint.h>
#include <arch/vm.h>

#define ELF_NIDENT    16

typedef struct elf32_header
{
    uint8_t     e_ident[ELF_NIDENT];
#define EI_MAG0         0 // 0x7F
#define EI_MAG1         1 // 'E'
#define EI_MAG2         2 // 'L'
#define EI_MAG3         3 // 'F'
#define EI_CLASS        4 // Architecture (32/64)
#define ELFCLASS32          1 // 32-bit Architecture
#define ELFCLASS64          2 // 64-bit Architecture
#define EI_DATA         5 // Byte Order
#define ELFDATA2LSB         1 // Little Endian
#define ELFDATA2MSB         2 // Big Endian
#define EI_VERSION      6 // ELF Version
#define EI_OSABI        7 // OS Specific
#define EI_ABIVERSION   8 // OS Specific

#define ELFMAG0    0x7F // e_ident[EI_MAG0]
#define ELFMAG1    'E'  // e_ident[EI_MAG1]
#define ELFMAG2    'L'  // e_ident[EI_MAG2]
#define ELFMAG3    'F'  // e_ident[EI_MAG3]

    uint16_t    e_type;
#define ET_NONE     0 // Unkown Type
#define ET_REL      1 // Relocatable File
#define ET_EXEC     2 // Executable File

    uint16_t    e_machine;
#define EM_386      3 // x86 Machine Type

    uint32_t    e_version;
#define EV_CURRENT  1 // ELF Current Version

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

int elf_check_file(struct elf32_header* hdr);
int read_elf(const char* name, void* data, vm_area_t** result_area, uint32_t* brk, void** entry);
