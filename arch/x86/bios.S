#include <arch/asm.h>
#include <arch/page.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/descriptor.h>

#define BIOS_BDA_START      0x400
#define CODE_ADDRESS        0x500
#define PROC_ADDRESS        0x600
#define GDTR_ADDRESS        0x610
#define GDTR_SIZE           0x6
#define GDT_ADDRESS         0x620
#define GDT_SIZE            24
#define IDT_ADDRESS         0x640
#define IDT_SIZE            0x6
#define GDTR32_ADDRESS      0x650
#define GDT32_ADDRESS       0x660
#define SAVED_STACK_ADDRESS 0x700
#define PARAMS_ADDRESS      0x800
#define PARAMS_SIZE         28
#define STACK_TOP           0x1000

#define REAL_ADDR(addr) \
    CODE_ADDRESS + (addr) - protected_16

#define REAL_ADDR_2(addr) \
    CODE_ADDRESS + (addr) - protected_16_2

SECTION(.data)

ENTRY(idt_real)
    .long 0x3ff
    .word 0
END(idt_real)

ENTRY(gdt16)

    // Null segment
    descriptor_entry(0, 0, 0)

    // Kernel 16 bit code segment
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_1B | GDT_FLAGS_16BIT, 0x0, 0xffff)

    // Kernel 16 bit data segment
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_1B | GDT_FLAGS_16BIT, 0x0, 0xffff)

END(gdt16)

ENTRY(gdtr16)
    .word . - gdt16 - 1
    .long GDT_ADDRESS
END(gdtr16)

SECTION(.text)

ENTRY(bda_ptr_get)
    mov $BIOS_BDA_START, %eax
    ret
ENDPROC(bda_ptr_get)

ENTRY(do_asm_memcpy)
    cld
    rep
    movsb
    ret
ENDPROC(do_asm_memcpy)

#define memcpy(dest, src, size) \
    mov dest, %edi; \
    mov src, %esi; \
    mov size, %ecx; \
    call do_asm_memcpy

// void bios_call(uint32_t function, struct regs *param)
ENTRY(bios_call)

    movl 4(%esp), %edx  // Function
    movl 8(%esp), %eax  // Address of regs structure
    pusha               // Save regs
    pushf               // Save flags and disable interrupts
    cli
    movl %esp, SAVED_STACK_ADDRESS // Save ESP

    memcpy($PARAMS_ADDRESS, %eax, $PARAMS_SIZE)
    memcpy($CODE_ADDRESS, $protected_16, $protected_paging_32 - protected_16)
    memcpy($GDTR_ADDRESS, $gdtr16, $GDTR_SIZE)
    memcpy($GDT_ADDRESS, $gdt16, $GDT_SIZE)
    memcpy($IDT_ADDRESS, $idt_real, $IDT_SIZE)
    memcpy($GDTR32_ADDRESS, $gdt, $GDTR_SIZE)
    memcpy($GDT32_ADDRESS, $gdt_entries, $GDT_SIZE)

    // Create actual int <fn> call:
    // cd <fn>  | int <fn>
    // c3       | ret
    movl $PROC_ADDRESS, %ebx
    shl $8, %edx
    or $0xc300cd, %edx
    mov %edx, (%ebx)

    lgdt (GDTR_ADDRESS)
    ljmp $KERNEL_CS, $CODE_ADDRESS

.code16
    protected_16:
        mov $KERNEL_DS, %eax // Set up segments
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs
        mov %ax, %ss

        // Set up stack pointer
        mov $STACK_TOP, %sp

        // Load empty interrupts vector
        lidt (IDT_ADDRESS)

        // Disable paging
        mov %cr0, %edx
        and $~CR0_PG, %edx
        mov %edx, %cr0
        xor %eax, %eax
        mov %eax, %cr3

        // Disable protected mode
        and $~CR0_PE, %edx
        mov %edx, %cr0

        // Jump to real mode
        ljmp $0, $REAL_ADDR(real_mode)

    real_mode:
        // Setup flat mem model
        xor %ax, %ax
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs
        mov %ax, %ss

        // Read registers
        mov $PARAMS_ADDRESS, %eax
        mov (%eax), %ebx
        push %ebx
        mov 4(%eax), %ebx
        mov 8(%eax), %ecx
        mov 12(%eax), %edx
        mov 16(%eax), %esi
        mov 20(%eax), %edi
        pop %eax

        // Perform call to BIOS
        push $PROC_ADDRESS
        push $REAL_ADDR(return)
        ljmp $0, $PROC_ADDRESS

    return:
        // Save registers
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

        lgdt (GDTR32_ADDRESS) // Load GDT for 32-bit protected mode
        mov %cr0, %edx
        or $CR0_PE, %edx  // Enable protected mode
        mov %edx, %cr0
        // Jump to 32-bit protected mode
        ljmp $KERNEL_CS, $REAL_ADDR(protected_32)

.code32
    protected_32:
        // Load page directory
        mov $page_dir, %eax
        movl %eax, %cr3

        // Enable paging
        mov %cr0, %eax
        or $CR0_PE | CR0_PG | CR0_WP, %eax
        mov %eax, %cr0
        mov $1f, %eax
        jmp *%eax

    1:
        // Set segment registers
        mov $KERNEL_DS, %eax
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs
        mov %ax, %ss

        // Load proper GDT back
        lgdt (gdt + KERNEL_PAGE_OFFSET)
        ljmp $KERNEL_CS, $protected_paging_32

    protected_paging_32:
        mov $idt + IDT_OFFSET, %eax
        lidt (%eax)
        movl SAVED_STACK_ADDRESS, %esp // Restore ESP
        popf
        popa // Restore regs
        push %esi
        push %edi
        memcpy(%eax, $PARAMS_ADDRESS, $PARAMS_SIZE)
        pop %edi
        pop %esi
        ret

ENDPROC(bios_call)

ENTRY(real_mode_reboot)
    cli
    memcpy($CODE_ADDRESS, $protected_16_2, $end - protected_16_2)
    memcpy($GDTR_ADDRESS, $gdtr16, $GDTR_SIZE)
    memcpy($GDT_ADDRESS, $gdt16, $GDT_SIZE)
    memcpy($IDT_ADDRESS, $idt_real, $IDT_SIZE)
    memcpy($GDTR32_ADDRESS, $gdt, $GDTR_SIZE)
    memcpy($GDT32_ADDRESS, $gdt_entries, $GDT_SIZE)

    lgdt (GDTR_ADDRESS)
    ljmp $KERNEL_CS, $CODE_ADDRESS

.code16
    protected_16_2:
        mov $KERNEL_DS, %eax // Set up segments
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs
        mov %ax, %ss

        // Set up stack pointer
        mov $STACK_TOP, %sp

        // Load empty interrupts vector
        lidt (IDT_ADDRESS)

        // Disable paging
        mov %cr0, %edx
        and $~CR0_PG, %edx
        mov %edx, %cr0
        xor %eax, %eax
        mov %eax, %cr3

        // Disable protected mode
        and $~CR0_PE, %edx
        mov %edx, %cr0

        // Jump to real mode
        ljmp $0, $REAL_ADDR_2(real_mode_2)

    real_mode_2:
        // Setup flat mem model
        xor %ax, %ax
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs
        mov %ax, %ss
        ljmp $0xffff, $0

    1:
        jmp 1b
    end:

ENDPROC(real_mode_reboot)
