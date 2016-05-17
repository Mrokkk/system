#include <arch/multiboot.h>
#include <arch/x86_asm.h>
#include <arch/register.h>
#include <arch/segment.h>

#define FLAGS         MULTIBOOT_HEADER_FLAGS
#define MAGIC         MULTIBOOT_HEADER_MAGIC
#define CHECKSUM     -(MAGIC + FLAGS)

SECTION(.multiboot)

ALIGN(4)

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

        /* Initialize page table *
         * (load pages for 4MB)  */
        mov $page0, %edi
        movl $0x07, %eax
    1:
        stosl
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

        /* Set up stack register */
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
        pushf               /* Place EFLAGS on the stack */
        pop %eax            /* Pop value from the stack to EAX */
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

    /* Knows CPUID */
    1:
        call_function(multiboot_read)
        push %eax
        call_function(kmain)

    2:
        jmp 2b

ENDPROC(_start)

.align 0x1000
.org 0x1000
ENTRY(page_dir)
    .long 0x103007
    .fill 1023, 4, 0

.org 0x2000
ENTRY(page0)
.org 0x3000


