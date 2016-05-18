#include <arch/multiboot.h>
#include <arch/x86_asm.h>
#include <arch/register.h>
#include <arch/segment.h>
#include <arch/descriptor.h>
#include <arch/page.h>

#define FLAGS         MULTIBOOT_HEADER_FLAGS
#define MAGIC         MULTIBOOT_HEADER_MAGIC
#define CHECKSUM     -(MAGIC + FLAGS)

SECTION(.multiboot)

ENTRY(multiboot_header)
    .long MAGIC
    .long FLAGS
    .long CHECKSUM
END(multiboot_header)

SECTION(.text.boot)

/*===========================================================================*
 *                                 _start                                    *
 *===========================================================================*/
ENTRY(_start)

    /* According to Multiboot specification, there's *
     * a magic number in EAX and pointer to          *
     * multiboot_info structure in EBX               */

        /* Zero EFLAGS register */
        pushl $0
        popfl

        /* Move pointer to the multiboot struct
         * to the EDX */
        mov %eax, %edx

        /* Set segment registers */
        mov $KERNEL_DS, %eax
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs

        /* Initialize page table *
         * (load pages for 4MB)  */
        mov $page0, %edi
        movl $PAGE_INIT_FLAGS, %eax
    1:  stosl
        add $0x1000, %eax
        cmp $1024 * 0x1000 + PAGE_INIT_FLAGS, %eax
        jne 1b

        /* Load page directory */
        mov $page_dir, %eax
        movl %eax, %cr3

        /* Enable paging */
        mov %cr0, %eax
        or $0x80000000, %eax
        mov %eax, %cr0
        mov $1f, %eax
        jmp *%eax

    1:  /* Load global descriptors table */
        lgdt (gdt + KERNEL_PAGE_OFFSET)
        ljmp $0x08, $1f

    1:  /* Set up stack register */
        movl $SYMBOL_NAME(init_process_stack), %esp
        addl $1024, %esp
        push %edx
        push %ebx

        /*
         * Clear BSS
         */
        movl $_sbss, %edi
        movl $_ebss, %ecx
        subl %edi, %ecx
        cld
        rep

        /*
         * Setup interrupt descriptors table
         */
        call SYMBOL_NAME(idt_configure)

        /*
         * Setup tss
         */
        call SYMBOL_NAME(tss_init)

        /*
         * Read multiboot structure
         * given by the bootloader
         */
        call SYMBOL_NAME(multiboot_read)

        /* multiboot_read returns address of
         * boot command line string, so we push
         * its value onto the stack
         */
        push %eax

        /* This is tricky - we jump to kmain
         * indirectly by returning from
         * page_dir_reload, which reloads
         * page directory
         */
        push $0
        push $SYMBOL_NAME(kmain)
        movl $0, page_dir
        jmp page_dir_flush

    /* Not reachable */
    2:
        jmp 2b

ENDPROC(_start)

ENTRY(gdt)
    .word 4096 * 8 - 1
    .long __gdt_entries + KERNEL_PAGE_OFFSET

ENTRY(__gdt_entries)

    /* Null segment */
    descriptor_entry(0, 0, 0)

    /* Kernel code segment */
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_4KB | \
                     GDT_FLAGS_32BIT, 0x0, 0xffffffff)

    /* Kernel data segment */
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_4KB | \
                     GDT_FLAGS_32BIT, 0x0, 0xffffffff)

    /* User code segment */
    descriptor_entry(GDT_FLAGS_RING3 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_4KB | \
                     GDT_FLAGS_32BIT, 0x0, 0xffffffff)

    /* User data segment */
    descriptor_entry(GDT_FLAGS_RING3 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_4KB | \
                     GDT_FLAGS_32BIT, 0x0, 0xffffffff)

    /* TSS */
    descriptor_entry(GDT_FLAGS_TYPE_32TSS, 0, 0)

ALIGN(0x1000)
ENTRY(page_dir)
    .long page_table0
    .fill KERNEL_PAGE_OFFSET / 0x400000 - 1, 4, 0
    .long page_table0

ALIGN(0x1000)
ENTRY(page0)
    .skip PAGE_INIT_FLAGS
page_table0:
    .skip 0x1000 - PAGE_INIT_FLAGS

