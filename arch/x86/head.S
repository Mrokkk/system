#include <arch/asm.h>
#include <arch/page.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/multiboot.h>
#include <arch/descriptor.h>

#define FLAGS       MULTIBOOT_HEADER_FLAGS_ALIGNED
#define MAGIC       MULTIBOOT_HEADER_MAGIC
#define CHECKSUM    -(MAGIC + FLAGS)

SECTION(.data.multiboot)

ENTRY(multiboot_header)
    .long MAGIC
    .long FLAGS
    .long CHECKSUM
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
        or $CR0_PE | CR0_PG | CR0_WP, %eax
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
        push $0

        call SYMBOL_NAME(kmain)

    // Not reachable
    1:  jmp 1b

ENDPROC(_start)

SECTION(.data.gdt)

ALIGN(32)
ENTRY(unpaged_gdt_entries)

    // Null segment
    descriptor_entry(0, 0, 0)

    // Kernel code segment
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_4KB | GDT_FLAGS_32BIT, 0x0, 0xffffffff)

    // Kernel data segment
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_4KB | GDT_FLAGS_32BIT, 0x0, 0xffffffff)

    // User code segment
    descriptor_entry(GDT_FLAGS_RING3 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_4KB | GDT_FLAGS_32BIT, 0x0, 0xffffffff)

    // User data segment
    descriptor_entry(GDT_FLAGS_RING3 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_4KB | GDT_FLAGS_32BIT, 0x0, 0xffffffff)

    // TSS
    descriptor_entry(GDT_FLAGS_TYPE_32TSS, 0, 0)

    // APM BIOS 32bit code
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_1B | GDT_FLAGS_32BIT, 0x0, 0xffff)

    // APM BIOS 16bit code
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_1B | GDT_FLAGS_16BIT, 0x0, 0xffff)

    // APM BIOS data
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_1B | GDT_FLAGS_32BIT, 0x0, 0xffff)

END(unpaged_gdt_entries)

ALIGN(32)
ENTRY(gdt)
    .word . - unpaged_gdt_entries - 1
    .long unpaged_gdt_entries + KERNEL_PAGE_OFFSET
END(gdt)

ALIGN(PAGE_SIZE)
SECTION(.data.pagetable)
ENTRY(page0)
    .fill PAGE_KERNEL_FLAGS, 1, 0
page_table0:
    .fill 0x4000 - PAGE_KERNEL_FLAGS, 1, 0
ALIGN(PAGE_SIZE)

SECTION(.data.pagedir)
ENTRY(page_dir)
    // First entry will be zeroed in paging_init; we need it available at boot
    // as kernel is running on unpaged memory
    .long page_table0
    .fill KERNEL_PAGE_OFFSET / (PAGE_SIZE * 1024) - 1, 4, 0
    .long page_table0
    .long page_table0 + PAGE_SIZE
    .long page_table0 + 2 * PAGE_SIZE
    .long page_table0 + 3 * PAGE_SIZE
    .fill 1024 - KERNEL_PAGE_OFFSET / (PAGE_SIZE * 1024) - 5, 4, 0
ALIGN(PAGE_SIZE)
