#pragma once

#include <stdint.h>

#define ELF_NIDENT    16

enum e_type
{
    ET_NONE     = 0, // Unkown Type
    ET_REL      = 1, // Relocatable File
    ET_EXEC     = 2, // Executable File
    ET_DYN      = 3, // Shared object file
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
#define SHT_DYNSYM     11 // Dynamic linker symbol table

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

#define ELF32_R_SYM(val)        ((val) >> 8)
#define ELF32_R_TYPE(val)       ((val) & 0xff)
#define ELF32_R_INFO(sym, type) (((sym) << 8) + ((type) & 0xff))

#define R_386_NONE          0       /* No reloc */
#define R_386_32            1       /* Direct 32 bit  */
#define R_386_PC32          2       /* PC relative 32 bit */
#define R_386_GOT32         3       /* 32 bit GOT entry */
#define R_386_PLT32         4       /* 32 bit PLT address */
#define R_386_COPY          5       /* Copy symbol at runtime */
#define R_386_GLOB_DAT      6       /* Create GOT entry */
#define R_386_JMP_SLOT      7       /* Create PLT entry */
#define R_386_RELATIVE      8       /* Adjust by program base */
#define R_386_GOTOFF        9       /* 32 bit offset to GOT */
#define R_386_GOTPC         10      /* 32 bit PC relative offset to GOT */
#define R_386_32PLT         11
#define R_386_GOT32X        43

typedef struct
{
    uint32_t    r_offset;
    uint32_t    r_info;
} elf32_rel_t;

typedef struct
{
    uint32_t      r_offset;
    uint32_t      r_info;
    uint32_t     r_addend;
} elf32_rela_t;

#define DT_NULL             0   /* Marks end of dynamic section */
#define DT_NEEDED           1   /* Name of needed library */
#define DT_PLTRELSZ         2   /* Size in bytes of PLT relocs */
#define DT_PLTGOT           3   /* Processor defined value */
#define DT_HASH             4   /* Address of symbol hash table */
#define DT_STRTAB           5   /* Address of string table */
#define DT_SYMTAB           6   /* Address of symbol table */
#define DT_RELA             7   /* Address of Rela relocs */
#define DT_RELASZ           8   /* Total size of Rela relocs */
#define DT_RELAENT          9   /* Size of one Rela reloc */
#define DT_STRSZ            10  /* Size of string table */
#define DT_SYMENT           11  /* Size of one symbol table entry */
#define DT_INIT             12  /* Address of init function */
#define DT_FINI             13  /* Address of termination function */
#define DT_SONAME           14  /* Name of shared object */
#define DT_RPATH            15  /* Library search path (deprecated) */
#define DT_SYMBOLIC         16  /* Start symbol search here */
#define DT_REL              17  /* Address of Rel relocs */
#define DT_RELSZ            18  /* Total size of Rel relocs */
#define DT_RELENT           19  /* Size of one Rel reloc */
#define DT_PLTREL           20  /* Type of reloc in PLT */
#define DT_DEBUG            21  /* For debugging; unspecified */
#define DT_TEXTREL          22  /* Reloc might modify .text */
#define DT_JMPREL           23  /* Address of PLT relocs */
#define DT_BIND_NOW         24  /* Process relocations of object */
#define DT_INIT_ARRAY       25  /* Array with addresses of init fct */
#define DT_FINI_ARRAY       26  /* Array with addresses of fini fct */
#define DT_INIT_ARRAYSZ     27  /* Size in bytes of DT_INIT_ARRAY */
#define DT_FINI_ARRAYSZ     28  /* Size in bytes of DT_FINI_ARRAY */
#define DT_RUNPATH          29  /* Library search path */
#define DT_FLAGS            30  /* Flags for the object being loaded */
#define DT_ENCODING         32  /* Start of encoded range */
#define DT_PREINIT_ARRAY    32  /* Array with addresses of preinit fct*/
#define DT_PREINIT_ARRAYSZ  33  /* size in bytes of DT_PREINIT_ARRAY */
#define DT_SYMTAB_SHNDX     34  /* Address of SYMTAB_SHNDX section */
#define DT_RELRSZ           35  /* Total size of RELR relative relocations */
#define DT_RELR             36  /* Address of RELR relative relocations */
#define DT_RELRENT          37  /* Size of one RELR relative relocaction */
#define DT_NUM              38  /* Number used */
#define DT_LOOS     0x6000000d  /* Start of OS-specific */
#define DT_HIOS     0x6ffff000  /* End of OS-specific */
#define DT_LOPROC   0x70000000  /* Start of processor-specific */
#define DT_HIPROC   0x7fffffff  /* End of processor-specific */

typedef struct
{
    int32_t d_tag;
    union
    {
        uint32_t d_val;
        uint32_t d_ptr;
        uint32_t d_off;
    }
    d_un;
} elf32_dyn_t;

#define AT_NULL     0   /* End of vector */
#define AT_IGNORE   1   /* Entry should be ignored */
#define AT_EXECFD   2   /* File descriptor of program */
#define AT_PHDR     3   /* Program headers for program */
#define AT_PHENT    4   /* Size of program header entry */
#define AT_PHNUM    5   /* Number of program headers */
#define AT_PAGESZ   6   /* System page size */
#define AT_BASE     7   /* Base address of interpreter */
#define AT_FLAGS    8   /* Flags */
#define AT_ENTRY    9   /* Entry point of program */
#define AT_NOTELF   10  /* Program is not ELF */
#define AT_UID      11  /* Real uid */
#define AT_EUID     12  /* Effective uid */
#define AT_GID      13  /* Real gid */
#define AT_EGID     14  /* Effective gid */
#define AT_CLKTCK   17  /* Frequency of times() */

/* Some more special a_type values describing the hardware.  */
#define AT_PLATFORM 15  /* String identifying platform.  */
#define AT_HWCAP    16  /* Machine-dependent hints about
                           processor capabilities.  */

/* This entry gives some information about the FPU initialization
   performed by the kernel.  */
#define AT_FPUCW    18  /* Used FPU control word.  */

/* Cache block sizes.  */
#define AT_DCACHEBSIZE  19  /* Data cache block size.  */
#define AT_ICACHEBSIZE  20  /* Instruction cache block size.  */
#define AT_UCACHEBSIZE  21  /* Unified cache block size.  */

/* A special ignored value for PPC, used by the kernel to control the
   interpretation of the AUXV. Must be > 16.  */
#define AT_IGNOREPPC    22  /* Entry should be ignored.  */

#define	AT_SECURE       23  /* Boolean, was exec setuid-like?  */

#define AT_BASE_PLATFORM 24 /* String identifying real platforms.*/

#define AT_RANDOM   25  /* Address of 16 random bytes.  */

#define AT_HWCAP2   26  /* More machine-dependent hints about
                           processor capabilities.  */

#define AT_RSEQ_FEATURE_SIZE    27  /* rseq supported feature size.  */
#define AT_RSEQ_ALIGN           28  /* rseq allocation alignment.  */

/* More machine-dependent hints about processor capabilities.  */
#define AT_HWCAP3   29  /* extension of AT_HWCAP.  */
#define AT_HWCAP4   30  /* extension of AT_HWCAP.  */

#define AT_EXECFN   31  /* Filename of executable.  */

/* Pointer to the global system page used for system calls and other
   nice things.  */
#define AT_SYSINFO      32
#define AT_SYSINFO_EHDR 33

/* Shapes of the caches.  Bits 0-3 contains associativity; bits 4-7 contains
   log2 of line size; mask those to get cache size.  */
#define AT_L1I_CACHESHAPE   34
#define AT_L1D_CACHESHAPE   35
#define AT_L2_CACHESHAPE    36
#define AT_L3_CACHESHAPE    37

/* Shapes of the caches, with more room to describe them.
   *GEOMETRY are comprised of cache line size in bytes in the bottom 16 bits
   and the cache associativity in the next 16 bits.  */
#define AT_L1I_CACHESIZE        40
#define AT_L1I_CACHEGEOMETRY    41
#define AT_L1D_CACHESIZE        42
#define AT_L1D_CACHEGEOMETRY    43
#define AT_L2_CACHESIZE         44
#define AT_L2_CACHEGEOMETRY     45
#define AT_L3_CACHESIZE         46
#define AT_L3_CACHEGEOMETRY     47

#define AT_MINSIGSTKSZ          51 /* Stack needed for signal delivery  */

typedef struct
{
    uint32_t a_type;    /* Entry type */
    union
    {
        uint32_t a_val; /* Integer value */
    } a_un;
} elf32_auxv_t;
