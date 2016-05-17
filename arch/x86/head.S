#include <arch/multiboot.h>
#include <arch/x86_asm.h>
#include <arch/register.h>
#include <arch/segment.h>
#include <arch/descriptor.h>

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
        movl $0x07, %eax
    1:  stosl
        add $0x1000, %eax
        cmp $1024*0x1000+7, %eax
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
        lgdt (gdt)
        ljmp $0x08, $1f

    1:  /* Set up stack register */
        movl $init_process_stack, %esp
        addl $1024, %esp
        push %edx
        push %ebx

        /* Clear BSS */
        movl $_sbss, %edi
        movl $_ebss, %ecx
        subl %edi, %ecx
        cld
        rep

        /* Check if our processor knows CPUID instruction */
        pushf       /* Place EFLAGS on the stack */
        pop %eax    /* Pop value from the stack to EAX */
        mov %eax, %ebx
        /* Set 23th bit in EFLAGS (ID) */
        xorl $register_bit_mask(eflags_id), %ebx
        push %ebx
        popf
        pushf
        pop %eax
        cmp %eax, %ecx
        jne 1f
        hlt    /* TODO: 486 processors */

    1:  /* Knows CPUID */

        /* Setup interrupt descriptors table */
        call idt_configure
        /* Setup tss */
        call tss_init
        call_function(multiboot_read)
        push %eax
        call_function(kmain)

    /* Not reachable */
    2:
        jmp 2b

ENDPROC(_start)

ENTRY(gdt)
    .word 4096 * 8 - 1
    .long __gdt_entries

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
                     GDT_FLAGS_32BIT, 0x0, 0xffff)

    /* TSS */
    descriptor_entry(GDT_FLAGS_TYPE_32TSS, 0, 0)

ALIGN(0x1000)
ENTRY(page_dir)
    .long page_table0
    .fill 1023, 4, 0

ENTRY(page0)
    .skip 7
page_table0:
    .skip 0x1000 - 7

