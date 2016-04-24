#include <arch/x86_asm.h>
#include <arch/segment.h>
#include <arch/descriptor.h>

#define HEAD_ADDRESS    0x500
#define HEAD_SIZE       0x80

#define RETURN_ADDRESS  0x580
#define RETURN_SIZE     0x80

#define PROC_ADDRESS    0x600
#define PROC_SIZE       0x100

#define GP_ADDRESS      0x700
#define GP_SIZE         0x6

#define GDT_ADDRESS     0x706
#define GDT_SIZE        24

#define IDT_ADDRESS     0x71e
#define IDT_SIZE        0x6

#define GP32_ADDRESS    0x724

#define GDT32_ADDRESS   0x72a

#define PARAMS_ADDRESS  0x800

#define STACK_TOP       0x1000

SECTION(.data)

ENTRY(pstack)
    .long 0
END(pstack)

ENTRY(idt_real)
    .long 0x3ff
    .word 0
END(idt_real)

ENTRY(gdt16)

    null16_descriptor:  /* 0x00 - null segment */
        .long 0, 0

    code16_descriptor:  /* 0x08 - kernel code segment */
        descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_1B | GDT_FLAGS_16BIT, 0x0, 0xffff)

    data16_descriptor:  /* 0x10 - kernel data segment */
        descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_1B | GDT_FLAGS_16BIT, 0x0, 0xffff)

END(gdt16)

ENTRY(gp16)
    .word .-gdt16-1
    .long GDT_ADDRESS
END(gp16)

SECTION(.text)

/*===========================================================================*
 *                                memcpy_asm                                 *
 *===========================================================================*/
ENTRY(memcpy_asm)
    mov 4(%esp), %edi
    mov 8(%esp), %esi
    mov 12(%esp), %ecx
    cld
    rep
    movsb
    ret
ENDPROC(memcpy_asm)

/*===========================================================================*
 *                           execute_in_real_mode                            *
 *===========================================================================*/
ENTRY(execute_in_real_mode)

        /* struct real_mode_regs_struct *execute_in_real_mode(    *
         *                  uint32_t function_address,            *
         *                  struct real_mode_regs_struct *param); */

        movl 4(%esp), %eax  /* Function pointer */
        movl 8(%esp), %ecx  /* Address of regs structure */
        pusha               /* Save regs */
        pushf
        cli
        mov %esp, pstack    /* Save ESP */
        push %ecx
        push %eax

        call SYMBOL_NAME(pic_disable)

        pop %eax
        call_function3(memcpy_asm, $PROC_ADDRESS, %eax, $PROC_SIZE)
        pop %eax
        call_function3(memcpy_asm, $PARAMS_ADDRESS, %eax, $28)
        call_function3(memcpy_asm, $HEAD_ADDRESS, $protected_16, $HEAD_SIZE)
        call_function3(memcpy_asm, $RETURN_ADDRESS, $return, $RETURN_SIZE)
        call_function3(memcpy_asm, $GP_ADDRESS, $gp16, $GP_SIZE)
        call_function3(memcpy_asm, $GDT_ADDRESS, $gdt16, $GDT_SIZE)
        call_function3(memcpy_asm, $IDT_ADDRESS, $idt_real, $IDT_SIZE)
        call_function3(memcpy_asm, $GP32_ADDRESS, $gdt, $GP_SIZE)
        call_function3(memcpy_asm, $GDT32_ADDRESS, $gdt_entries, $GDT_SIZE)

        lgdt (GP_ADDRESS)
        ljmp $0x08, $HEAD_ADDRESS

.code16

    protected_16:
        xor %eax, %eax
        mov $0x10, %ax /* Set up segments */
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs
        mov %ax, %ss
        mov $STACK_TOP, %sp /* Set esp */

        lidt (IDT_ADDRESS) /* Load interrupts vector */
        mov $0, %eax
        mov %eax, %cr3  /* Clear CR3 */
        mov %cr0, %edx  /* Clear bit 0 in CR0 (PE) */
        sub $1, %edx
        mov %edx, %cr0
        ljmp $0, $real_mode - protected_16 + HEAD_ADDRESS /* Jump to real mode */

    real_mode:
        mov $0, %ax /* Setup flat mem model */
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs
        mov %ax, %ss
        mov $PARAMS_ADDRESS, %eax
        /* Read registers */
        mov (%eax), %ebx
        push %ebx
        mov 4(%eax), %ebx
        mov 8(%eax), %ecx
        mov 12(%eax), %edx
        mov 16(%eax), %esi
        mov 20(%eax), %edi
        pop %eax
        push $PROC_ADDRESS
        push $RETURN_ADDRESS
        sti
        ljmp $0, $PROC_ADDRESS

    return:
        cli
        mov $PARAMS_ADDRESS, %ebp
        mov %eax, (%ebp)
        mov %ebx, 4(%ebp)
        mov %ecx, 8(%ebp)
        mov %edx, 12(%ebp)
        mov %esi, 16(%ebp)
        mov %edi, 20(%ebp)
        push %eax
        pushf
        pop %eax
        mov %eax, 24(%ebp)
        pop %eax
        add $4, %sp

        lgdt (GP32_ADDRESS) /* Read GDT for 32-bit protected mode */
        mov %cr0, %edx
        add $1, %edx  /* Set PE bit in CR0 */
        mov %edx, %cr0
        /* Jump to 32-bit protected mode */
        ljmp $0x08, $protected_again - return + RETURN_ADDRESS

.code32

    protected_again:
        mov $KERNEL_DS, %edx
        mov %dx, %ds
        mov %dx, %es
        mov %dx, %fs
        mov %dx, %gs
        mov %dx, %ss
        lgdt (gdt)
        ljmp $0x08, $return2

    return2:
        lidt (idt)
        call_function(pic_enable)
        mov pstack, %esp    /* Restore ESP */
        popf
        popa                /* Restore regs */
        mov 8(%esp), %eax
        pusha
        call_function3(memcpy_asm, %eax, $PARAMS_ADDRESS, $28)
        popa

        ret

ENDPROC(execute_in_real_mode)
