#include <arch/asm.h>
#include <arch/msr.h>
#include <arch/bios.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/real_mode.h>
#include <arch/page_table.h>
#include <arch/page_types.h>
#include <arch/descriptor.h>

#define REAL_ADDR(addr) \
    CODE_ADDRESS + (addr) - protected_16

SECTION(.text)

ENTRY(null_get)
    mov $0, %eax
    ret
ENDPROC(null_get)

ENTRY(bda_ptr_get)
    mov $BIOS_BDA_START, %eax
    ret
ENDPROC(bda_ptr_get)

ENTRY(real_mode_call)

#ifdef __x86_64__
    SAVE_ALL(0)
#else
    push %gs
    push %fs
    push %es
    push %ds
    pusha
#endif
    pushf
    cli

#ifdef __x86_64__
    mov %rsp, SAVED_STACK_ADDRESS

    mov $jmp_comp_mode - KERNEL_PAGE_OFFSET, %rax
    ljmp *(%rax)

jmp_comp_mode:
    .long compatible_mode - KERNEL_PAGE_OFFSET
    .word KERNEL_32_CS

.code32
compatible_mode:
    // Disable paging
    mov %cr0, %edx
    and $~CR0_PG, %edx
    mov %edx, %cr0
    xor %eax, %eax
    mov %eax, %cr3

    ljmp $KERNEL_32_CS, $1f - KERNEL_PAGE_OFFSET

1:
    mov $IA32_MSR_EFER, %ecx
    rdmsr
    and $~IA32_MSR_EFER_LME, %eax
    wrmsr

    ljmp $KERNEL_16_CS, $REAL_ADDR(protected_16)
#else
    mov %esp, SAVED_STACK_ADDRESS
    ljmp $KERNEL_16_CS, $REAL_ADDR(protected_16)
#endif

.code16
protected_16:
    // Set up segments
    SEGMENT_REGISTERS_SET(KERNEL_16_DS, ax)

    // Set up stack pointer
    mov $STACK_TOP, %sp

    // Load real mode interrupts vector
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
    SEGMENT_REGISTERS_SET(0, ax)

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

    // Call code set by the client
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
    mov %es, %eax
    mov %eax, 28(%ebp)
    pop %eax
    add $4, %sp

    lgdt (GDT_ADDRESS) // Load GDT for 32-bit protected mode
    mov %cr0, %edx
    or $CR0_PE, %edx  // Enable protected mode
    mov %edx, %cr0
    // Jump to 32-bit protected mode
#ifdef __x86_64__
    ljmp $KERNEL_32_CS, $REAL_ADDR(protected_32)
#else
    ljmp $KERNEL_CS, $REAL_ADDR(protected_32)
#endif

.code32
protected_32:
#ifdef __x86_64__
    SEGMENT_REGISTERS_SET(KERNEL_DS, ax)

    mov $IA32_MSR_EFER, %ecx
    rdmsr
    or $IA32_MSR_EFER_LME, %eax
    wrmsr

    // Load page directory
    mov $pgt4, %eax
    movl %eax, %cr3

    // Enable paging
    mov %cr0, %eax
    or $CR0_PG, %eax
    mov %eax, %cr0

    lgdt (gdt - KERNEL_PAGE_OFFSET)
    ljmp $KERNEL_CS, $long_mode - KERNEL_PAGE_OFFSET

real_mode_call_end:

.code64
long_mode:
    SEGMENT_REGISTERS_SET(KERNEL_DS, ax)

    movabs $idt, %rax
    lidt (%rax)

    // Reload global descriptors table (at virtual address)
    movabs $gdt, %rax
    lgdt (%rax)

    movabs $jmp_label, %rax
    rex.w ljmp *(%rax)

jmp_label:
    .quad 1f
    .word KERNEL_CS

1:  mov SAVED_STACK_ADDRESS, %rsp
    popf
    RESTORE_ALL
    ret

#else
    // Load page directory
    mov $kernel_page_dir - KERNEL_PAGE_OFFSET, %eax
    mov %eax, %cr3

    // Enable paging
    mov %cr0, %eax
    or $CR0_PG, %eax
    mov %eax, %cr0
    mov $1f, %eax
    jmp *%eax

1:  // Set segment registers
    SEGMENT_REGISTERS_SET(KERNEL_DS, ax)

    // Load proper GDT back
    lgdt (gdt)
    ljmp $KERNEL_CS, $protected_paging_32
real_mode_call_end:

protected_paging_32:
    mov $idt, %eax
    lidt (%eax)
    mov SAVED_STACK_ADDRESS, %esp
    popf
    popa
    pop %ds
    pop %es
    pop %fs
    pop %gs
    ret
#endif

ENDPROC(real_mode_call)

SECTION(.data)

ENTRY(real_mode_code_address)
    .dc.a protected_16
END(real_mode_code_address)

ENTRY(real_mode_code_size)
    .long real_mode_call_end - protected_16
END(real_mode_code_size)
