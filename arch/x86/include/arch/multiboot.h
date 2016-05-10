#ifndef __MULTIBOOT_H_
#define __MULTIBOOT_H_

#define MULTIBOOT_HEADER_MAGIC          0x1BADB002

#define MULTIBOOT_HEADER_FLAGS_ALIGNED  (1 << 0)
#define MULTIBOOT_HEADER_FLAGS_MEMORY   (1 << 1)
#define MULTIBOOT_HEADER_FLAGS_VIDEO    (1 << 2)
#define MULTIBOOT_HEADER_FLAGS_ADDRESS  (1 << 16)

#ifdef __ELF__

#define MULTIBOOT_HEADER_FLAGS \
    MULTIBOOT_HEADER_FLAGS_ALIGNED | MULTIBOOT_HEADER_FLAGS_MEMORY | \
    MULTIBOOT_HEADER_FLAGS_VIDEO

#else /* __ELF__ */

#define MULTIBOOT_HEADER_FLAGS \
    MULTIBOOT_HEADER_FLAGS_ALIGNED | MULTIBOOT_HEADER_FLAGS_MEMORY | \
    MULTIBOOT_HEADER_FLAGS_VIDEO | MULTIBOOT_HEADER_FLAGS_ADDRESS

#endif /* __ELF__ */

/* Magic number passed from Multiboot 1 compliant bootloader */
#define MULTIBOOT_BOOTLOADER_MAGIC  0x2BADB002

#define MULTIBOOT_VIDEO_MODE_LIN    0
#define MULTIBOOT_VIDEO_MODE_EGA    1
#define MULTIBOOT_VIDEO_MODE_WIDTH  80
#define MULTIBOOT_VIDEO_MODE_HEIGHT 25

#ifndef __ASSEMBLER__

/* The Multiboot header. */
struct multiboot_header {
    unsigned long magic;
    unsigned long flags;
    unsigned long checksum;
    unsigned long header_addr;
    unsigned long load_addr;
    unsigned long load_end_addr;
    unsigned long bss_end_addr;
    unsigned long entry_addr;
};

/* The symbol table for a.out. */
struct aout_symbol_table {
    unsigned long tabsize;
    unsigned long strsize;
    unsigned long addr;
    unsigned long reserved;
};

/* The section header table for ELF. */
struct elf_section_header_table {
    unsigned long num;
    unsigned long size;
    unsigned long addr;
    unsigned long shndx;
};

/* The Multiboot information. */
struct multiboot_info {
    unsigned long flags;
    unsigned long mem_lower;
    unsigned long mem_upper;
    unsigned long boot_device;
    unsigned long cmdline;
    unsigned long mods_count;
    unsigned long mods_addr;
    union {
        struct aout_symbol_table aout_sym;
        struct elf_section_header_table elf_sec;
    };
    unsigned long mmap_length;
    unsigned long mmap_addr;
    unsigned long drives_length;
    unsigned long drives_addr;
    unsigned long config_table;
    unsigned long bootloader_name;
    unsigned long apm_table;
    struct vbe_struct {
        unsigned long control_info;
        unsigned long mode_info;
        unsigned long mode;
        unsigned long interface_seg;
        unsigned long interface_off;
        unsigned long interface_len;
    } vbe;
};

/* The module structure. */
struct module {
    unsigned long mod_start;
    unsigned long mod_end;
    unsigned long string;
    unsigned long reserved;
};

/* The memory map. Be careful that the offset 0 is base_addr_low
but no size. */
struct memory_map {
    unsigned long size;
    unsigned long base_addr_low;
    unsigned long base_addr_high;
    unsigned long length_low;
    unsigned long length_high;
    unsigned long type;
};

struct multiboot_drives_struct {
    unsigned int size;
    unsigned char drive_number;
    unsigned char drive_mode;
    unsigned short drive_cylinders;
    unsigned char drive_heads;
    unsigned char drive_sectors;
    unsigned char drive_ports;
};

struct multiboot_apm_table_struct {
    unsigned short version;
    unsigned short cseg;
    unsigned int offset;
    unsigned short cseg_16;
    unsigned short dseg;
    unsigned short flags;
    unsigned short cseg_len;
    unsigned short cseg_16_len;
    unsigned short dseg_len;
};

#define MULTIBOOT_FLAGS_MEM_BIT             (1 << 0)
#define MULTIBOOT_FLAGS_BOOTDEV_BIT         (1 << 1)
#define MULTIBOOT_FLAGS_CMDLINE_BIT         (1 << 2)
#define MULTIBOOT_FLAGS_MODS_BIT            (1 << 3)
#define MULTIBOOT_FLAGS_SYMS_BIT            ((1 << 4) | (1 << 5))
#define MULTIBOOT_FLAGS_MMAP_BIT            (1 << 6)
#define MULTIBOOT_FLAGS_DRIVES_BIT          (1 << 7)
#define MULTIBOOT_FLAGS_CONFIG_TABLE_BIT    (1 << 8)
#define MULTIBOOT_FLAGS_BL_NAME_BIT         (1 << 9)
#define MULTIBOOT_FLAGS_APM_TABLE_BIT       (1 << 10)
#define MULTIBOOT_FLAGS_VBE_BIT             (1 << 11)

int multiboot_read(struct multiboot_info *mb, unsigned int magic);

#endif /* __ASSEMBLER__ */

#endif /* __MULTIBOOT_H_ */
