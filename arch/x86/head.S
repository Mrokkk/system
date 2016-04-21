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
    .long 0
    .long 0
    .long 0
    .long 0
    .long MULTIBOOT_VIDEO_MODE_LIN
    .long MULTIBOOT_VIDEO_MODE_WIDTH
    .long MULTIBOOT_VIDEO_MODE_HEIGHT
    .long 0
END(multiboot_header)

SECTION(.text)

/*===========================================================================*
 *                                 _start                                    *
 *===========================================================================*/
ENTRY(_start)

    /* According to Multiboot specification, there's *
     * a magic number in EAX and pointer to          *
     * multiboot_info structure in EBX               */

    /* Zero EFLAGS register */
    setup_eflags:
        pushl $0
        popfl

    /* Set up stack register */
    setup_stack:
        movl $init_stack, %esp
        addl $1024, %esp
        push %eax
        push %ebx

    /* Clear BSS */
    clear_bss:
        movl $_sbss, %edi
        movl $_ebss, %ecx
        subl %edi, %ecx
        cld
        rep

    /* Check if our processor knows CPUID instruction */
    check_cpuid:
        pushf               /* Place EFLAGS on the stack */
        pop %eax            /* Pop value from the stack to EAX */
        mov %eax, %ebx
        xorl $register_bit_mask(eflags_id), %ebx /* Set 23th bit in EFLAGS (ID) */
        push %ebx
        popf
        pushf
        pop %eax
        cmp %eax, %ecx
        jne _arch_setup

    is_486:
        hlt    /* TODO */

    /* Knows CPUID */
    _arch_setup:
        call_function(multiboot_read)
        call_function(arch_setup)

    /* Check if bootloader was Multiboot compliant */
    check_mb_magic:
        pop %ebx
        pop %eax
        cmp $0x2BADB002, %eax
        je jump_to_main
        call_function1(printk, $bad_magic_msg)

    jump_to_main:
        call_function(kmain)

    inf_loop:
        jmp inf_loop

ENDPROC(_start)

ENTRY(bad_magic_msg)
    .asciz "check_mb_magic: Not Multiboot v1 compilant bootloader!\n"
END(bad_magic_msg)

