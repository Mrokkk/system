#include <arch/asm.h>
#include <arch/msr.h>
#include <arch/bios.h>
#include <arch/page.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/descriptor.h>

#include <kernel/segmexec.h>

SECTION(.text.boot)

.code32
ENTRY(_start)

    // eax - multiboot magic number
    // ebx - ptr to multiboot_info

    // Zero EFLAGS register
    pushl $0
    popfl

    // Save multiboot data
    push %eax
    push %ebx

    call bss_zero
    call long_mode_check
    call paging_setup

    // Load global descriptors table (at physical
    // address) and jump to long mode
    lgdt (gdt - KERNEL_PAGE_OFFSET)
    ljmp $KERNEL_CS, $long_mode

long_mode_check:
    mov $0x80000000, %eax
    cpuid

    cmp $0x80000001, %eax
    jb no_long_mode

    mov $0x80000001, %eax
    cpuid
    test $1 << 29, %edx
    jz no_long_mode

    ret

no_long_mode:
    mov $message, %esi
    call print32
1:  hlt
    jmp 1b

bss_zero:
    mov $0, %eax
    mov $_bss_phys_start, %edi
    mov $_bss_phys_end, %ecx
    sub $_bss_phys_start, %ecx
    rep stosb
    ret

print32:
    mov $0xb8000, %edi
    mov $(7 << 8), %ebx
1:
    cmpb $0, (%esi)
    jz 1f
    movb (%esi), %bl
    movw %bx, (%edi)
    inc %esi
    add $2, %edi
    jmp 1b
1:  ret

paging_setup:
    mov $pgt1, %edi
    mov $PAGE_KERNEL_FLAGS, %eax

1:  // TODO: put here _end - KERNEL_PAGE_OFFSET + PAGE_KERNEL_FLAGS
    cmp $4 * 1024 * 1024 + PAGE_KERNEL_FLAGS, %eax
    jg 1f

    mov %eax, (%edi)
    add $8, %edi
    add $PAGE_SIZE, %eax
    jmp 1b

1:  lea pgt4, %edi
    mov $pgt3, %eax
    or $PAGE_KERNEL_FLAGS, %eax
    mov %eax, (%edi)
    mov %eax, 0x800(%edi) // KERNEL_PTE4_OFFSET * sizeof(uint64_t)

    mov $pgt2, %eax
    or $PAGE_KERNEL_FLAGS, %eax
    mov %eax, pgt3

    mov $pgt1, %eax
    or $PAGE_KERNEL_FLAGS, %eax
    mov %eax, pgt2

    mov %cr4, %eax
    or $CR4_PAE, %eax
    mov %eax, %cr4

    // Enable long mode and NX paging
    mov $IA32_MSR_EFER, %ecx
    rdmsr
    or $IA32_MSR_EFER_LME | IA32_MSR_EFER_NXE, %eax
    wrmsr

    // Load page directory
    mov $pgt4, %eax
    movl %eax, %cr3

    // Enable paging
    mov %cr0, %eax
    or $CR0_PE | CR0_PG | CR0_WP, %eax
    mov %eax, %cr0
    pop %eax
    jmp *%eax

.code64
long_mode:
    // Set segment registers
    SEGMENT_REGISTERS_SET(KERNEL_DS, ax)

    // Restore multiboot data from old stack
    mov $0, %rdi
    xor %rsi, %rsi
    xor %rdx, %rdx
    movl (%rsp), %esi // multiboot_info
    movl 4(%rsp), %edx // multiboot magic

    // Setup new stack
    movabs $init_stack, %rsp
    add $PAGE_SIZE, %rsp

    movabs $gdt, %r8
    movabs $gdt_entries, %r9
    mov %r9, 2(%r8)

    // Reload global descriptors table (at virtual address)
    movabs $gdt, %rax
    lgdt (%rax)

    movabs $jmp_kmain, %rax
    rex.w ljmp *(%rax)

jmp_kmain:
    .quad kmain
    .word KERNEL_CS

message:
    .string "CPU does not support long mode!\0"

ENDPROC(_start)

SECTION(.data.gdt)

ALIGN(32)
ENTRY(gdt_entries)

    // Null segment
    descriptor_entry(0, 0, 0, 0)

    // Kernel code segment
    descriptor_entry(DESC_ACCESS_CODE | DESC_ACCESS_RING(0), DESC_FLAGS_64BIT, 0, 0)

    // Kernel data segment (both 64 and 32 bits)
    descriptor_entry(DESC_ACCESS_DATA | DESC_ACCESS_RING(0), DESC_FLAGS_4KB | DESC_FLAGS_32BIT, 0, 0xfffff)

    // User code segment
    descriptor_entry(DESC_ACCESS_CODE | DESC_ACCESS_RING(3), DESC_FLAGS_64BIT, 0, 0)

    // User data segment
    descriptor_entry(DESC_ACCESS_DATA | DESC_ACCESS_RING(3), 0, 0, 0)

    // APM BIOS 32bit code
    descriptor_entry(DESC_ACCESS_CODE | DESC_ACCESS_RING(0), DESC_FLAGS_1B | DESC_FLAGS_32BIT, 0, 0xffff)

    // APM BIOS 16bit code
    descriptor_entry(DESC_ACCESS_CODE | DESC_ACCESS_RING(0), DESC_FLAGS_1B | DESC_FLAGS_16BIT, 0, 0xffff)

    // APM BIOS data
    descriptor_entry(DESC_ACCESS_DATA | DESC_ACCESS_RING(0), DESC_FLAGS_1B | DESC_FLAGS_32BIT, 0, 0xffff)

    // User TLS segment
    descriptor_entry(DESC_ACCESS_DATA | DESC_ACCESS_RING(3), DESC_FLAGS_4KB | DESC_FLAGS_32BIT, 0, 0)

    // Kernel code segment
    descriptor_entry(DESC_ACCESS_CODE | DESC_ACCESS_RING(0), DESC_FLAGS_4KB | DESC_FLAGS_32BIT, 0, 0xfffff)

    // Kernel 16 bit code segment
    descriptor_entry(DESC_ACCESS_CODE | DESC_ACCESS_RING(0), DESC_FLAGS_1B | DESC_FLAGS_16BIT, 0, 0xffff)

    // Kernel 16 bit data segment
    descriptor_entry(DESC_ACCESS_DATA | DESC_ACCESS_RING(0), DESC_FLAGS_1B | DESC_FLAGS_16BIT, 0, 0xffff)

    // TSS
    descriptor_entry(DESC_ACCESS_TSS | DESC_ACCESS_RING(0), 0, 0, 0)

END(gdt_entries)

ALIGN(32)
ENTRY(gdt)
    .word . - gdt_entries - 1
    .quad gdt_entries - KERNEL_PAGE_OFFSET
END(gdt)

SECTION(.data.pagetable)
ALIGN(PAGE_SIZE)
ENTRY(pgt1)
    .space PAGE_SIZE

ENTRY(pgt2)
    .space PAGE_SIZE

ENTRY(pgt3)
    .space PAGE_SIZE

ENTRY(pgt4)
ENTRY(kernel_page_dir)
    .space PAGE_SIZE
