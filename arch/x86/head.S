#include <arch/asm.h>
#include <arch/page.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/multiboot.h>
#include <arch/descriptor.h>

#define FLAGS       MULTIBOOT_HEADER_FLAGS_ALIGNED | MULTIBOOT_HEADER_FLAGS_MEMORY | MULTIBOOT_HEADER_FLAGS_VIDEO
#define MAGIC       MULTIBOOT_HEADER_MAGIC
#define CHECKSUM    -(MAGIC + FLAGS)

SECTION(.multiboot)

ENTRY(multiboot_header)
    .long MAGIC
    .long FLAGS
    .long CHECKSUM
    .long 0 // header_addr
    .long 0 // load_addr
    .long 0 // load_end_addr
    .long 0 // bss_end_addr
    .long 0 // entry_addr
    .long MULTIBOOT_VIDEO_MODE_LIN // mode_type
    .long 1024 // width
    .long 768 // height
    .long 32 // depth
END(multiboot_header)

SECTION(.text.boot)

ENTRY(_start)

    // According to Multiboot specification, there's
    // a magic number in EAX and pointer to
    // multiboot_info structure in EBX

        // Zero EFLAGS register
        pushl $0
        popfl

        // Move pointer to the multiboot struct to the EDX
        mov %eax, %edx

        // Initialize page table to cover whole loaded memory
        mov $page0, %edi
        movl $PAGE_KERNEL_FLAGS, %eax
    1:  stosl
        add $PAGE_SIZE, %eax
        cmp $_end - KERNEL_PAGE_OFFSET + PAGE_KERNEL_FLAGS, %eax
        jne 1b

        // Load page directory
        mov $page_dir, %eax
        movl %eax, %cr3

        // Enable paging
        mov %cr0, %eax
        or $0x80000000, %eax
        mov %eax, %cr0
        mov $1f, %eax
        jmp *%eax

    1:  // Load global descriptors table
        lgdt (gdt + KERNEL_PAGE_OFFSET)

        // Set segment registers
        mov $KERNEL_DS, %eax
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs
        mov %ax, %ss

        ljmp $KERNEL_CS, $1f

    1:  // Set up stack register
        movl $SYMBOL_NAME(init_process_stack), %esp
        addl $INIT_PROCESS_STACK_SIZE * 4, %esp

        // Push multiboot data on stack
        push %edx
        push %ebx

        call SYMBOL_NAME(kmain)

    // Not reachable
    2:
        jmp 2b

ENDPROC(_start)

ENTRY(gdt)
    .word 4096 * 8 - 1
    .long __gdt_entries + KERNEL_PAGE_OFFSET

ENTRY(__gdt_entries)

    // Null segment
    descriptor_entry(0, 0, 0)

    // Kernel code segment
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_4KB | \
                     GDT_FLAGS_32BIT, 0x0, 0xffffffff)

    // Kernel data segment
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_4KB | \
                     GDT_FLAGS_32BIT, 0x0, 0xffffffff)

    // User code segment
    descriptor_entry(GDT_FLAGS_RING3 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_4KB | \
                     GDT_FLAGS_32BIT, 0x0, 0xffffffff)

    // User data segment
    descriptor_entry(GDT_FLAGS_RING3 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_4KB | \
                     GDT_FLAGS_32BIT, 0x0, 0xffffffff)

    // TSS
    descriptor_entry(GDT_FLAGS_TYPE_32TSS, 0, 0)

ALIGN(PAGE_SIZE)
ENTRY(page_dir)
    // First entry will be zeroed in paging_init; we need it available at boot
    .long page_table0
    .fill KERNEL_PAGE_OFFSET / (PAGE_SIZE * 1024) - 1, 4, 0
    .long page_table0

ALIGN(PAGE_SIZE)
ENTRY(page0)
    .skip PAGE_KERNEL_FLAGS
page_table0:
    .skip 0x1000 - PAGE_KERNEL_FLAGS
