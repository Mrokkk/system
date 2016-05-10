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

        /* Set up stack register */
        movl $init_process_stack, %esp
        addl $1024, %esp
        push %eax
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

