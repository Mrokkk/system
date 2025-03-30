#include <arch/asm.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/descriptor.h>
#include <arch/page_table.h>
#include <arch/page_types.h>

#include <kernel/segmexec.h>

SECTION(.text.boot)

ENTRY(_start)

    // eax - multiboot magic number
    // ebx - ptr to multiboot_info
    mov %eax, %edx

    // Zero EFLAGS register
    pushl $0
    popfl

    call bss_zero
    call paging_setup
    call gdt_setup
    call stack_setup
    call cpuid_check

    // Push multiboot data on stack
    push %edx
    push %ebx
    push $0

    call SYMBOL_NAME(kmain)

    // Not reachable
1:  jmp 1b

bss_zero:
    mov $0, %eax
    mov $_bss_start, %edi
    mov $_bss_end, %ecx
    sub $_bss_start, %ecx
    rep stosb
    ret

paging_setup:
    // Initialize page table to cover whole loaded memory
    mov $page0, %edi
    movl $PAGE_KERNEL_FLAGS, %eax
1:  stosl
    add $PAGE_SIZE, %eax
    cmp $_end - KERNEL_PAGE_OFFSET + PAGE_KERNEL_FLAGS, %eax
    jne 1b

    // Load page directory
    mov $kernel_page_dir - KERNEL_PAGE_OFFSET, %eax
    movl %eax, %cr3

    // Enable paging and enable caching
    mov %cr0, %eax
    or $CR0_PE | CR0_PG | CR0_WP, %eax
    and $~(CR0_NW | CR0_CD), %eax
    mov %eax, %cr0
    pop %eax
    jmp *%eax

gdt_setup:
    // Load global descriptors table
    lgdt (gdt)
    ljmp $KERNEL_CS, $1f

1:  // Set segment registers
    SEGMENT_REGISTERS_SET(KERNEL_DS, ax)
    ret

stack_setup:
    mov (%esp), %eax
    // Setup new stack
    movl $SYMBOL_NAME(init_process_stack), %esp
    addl $INIT_PROCESS_STACK_SIZE * 4, %esp
    jmp *%eax

cpuid_check:
    movl $3, family
    call get_flags
    mov %eax, %ecx
    xor $EFL_AC, %eax
    call set_flags
    call get_flags
    xor %ecx, %eax
    jz no_cpuid

    call restore_flags

    movl $4, family
    call get_flags
    mov %eax, %ecx
    xor $EFL_ID, %eax
    call set_flags
    call get_flags
    xor %ecx, %eax
    jz no_cpuid

has_cpuid:
    movl $1, cpuid_available

no_cpuid:
    call restore_flags
    ret

get_flags:
    pushfl
    pop %eax
    ret

set_flags:
    push %eax
    popfl
    ret

restore_flags:
    push %ecx
    popfl
    ret

ENDPROC(_start)

SECTION(.data.gdt)

ALIGN(8)
ENTRY(gdt)
    .word gdt_entries_end - gdt_entries - 1
    .long gdt_entries
END(gdt)

ALIGN(8)
ENTRY(gdt_entries)

    // Null segment
    descriptor_entry(0, 0, 0, 0)

    // Kernel code segment
    descriptor_entry(DESC_ACCESS_CODE | DESC_ACCESS_RING(0), DESC_FLAGS_4KB | DESC_FLAGS_32BIT, 0, 0xfffff)

    // Kernel data segment
    descriptor_entry(DESC_ACCESS_DATA | DESC_ACCESS_RING(0), DESC_FLAGS_4KB | DESC_FLAGS_32BIT, 0, 0xfffff)

#if CONFIG_SEGMEXEC
    // User code segment
    descriptor_entry(DESC_ACCESS_CODE | DESC_ACCESS_RING(3), DESC_FLAGS_4KB | DESC_FLAGS_32BIT, CODE_START, 0xfffff)

    // User data segment
    descriptor_entry(DESC_ACCESS_DATA | DESC_ACCESS_RING(3), DESC_FLAGS_4KB | DESC_FLAGS_32BIT, 0, CODE_START / 0x1000)
#else
    // User code segment
    descriptor_entry(DESC_ACCESS_CODE | DESC_ACCESS_RING(3), DESC_FLAGS_4KB | DESC_FLAGS_32BIT, 0, 0xfffff)

    // User data segment
    descriptor_entry(DESC_ACCESS_DATA | DESC_ACCESS_RING(3), DESC_FLAGS_4KB | DESC_FLAGS_32BIT, 0, 0xfffff)
#endif

    // APM BIOS 32bit code
    descriptor_entry(DESC_ACCESS_CODE | DESC_ACCESS_RING(0), DESC_FLAGS_1B | DESC_FLAGS_32BIT, 0, 0xfffff)

    // APM BIOS 16bit code
    descriptor_entry(DESC_ACCESS_CODE | DESC_ACCESS_RING(0), DESC_FLAGS_1B | DESC_FLAGS_16BIT, 0, 0xffff)

    // APM BIOS data
    descriptor_entry(DESC_ACCESS_DATA | DESC_ACCESS_RING(0), DESC_FLAGS_1B | DESC_FLAGS_32BIT, 0, 0xffff)

    // User TLS segment
    descriptor_entry(DESC_ACCESS_DATA | DESC_ACCESS_RING(3), DESC_FLAGS_4KB | DESC_FLAGS_32BIT, 0, 0)

    // Kernel 16 bit code segment
    descriptor_entry(DESC_ACCESS_CODE | DESC_ACCESS_RING(0), DESC_FLAGS_1B | DESC_FLAGS_16BIT, 0, 0xffff)

    // Kernel 16 bit data segment
    descriptor_entry(DESC_ACCESS_DATA | DESC_ACCESS_RING(0), DESC_FLAGS_1B | DESC_FLAGS_16BIT, 0, 0xffff)

    // Kernel per CPU data segment
    descriptor_entry(DESC_ACCESS_DATA | DESC_ACCESS_RING(0), DESC_FLAGS_4KB | DESC_FLAGS_32BIT, 0, 0xfffff)

    // TSS
    descriptor_entry(DESC_ACCESS_TSS | DESC_ACCESS_RING(0), 0, 0, 0)

gdt_entries_end:
.global gdt_entries_end
END(gdt_entries)

SECTION(.data.pagetable)
ALIGN(PAGE_SIZE)
ENTRY(page0)
    .space PAGE_KERNEL_FLAGS
page_table0:
    .space 8 * PAGE_SIZE - PAGE_KERNEL_FLAGS
ALIGN(PAGE_SIZE)

END(page0)

SECTION(.data.pagedir)
ALIGN(PAGE_SIZE)
ENTRY(kernel_page_dir)
    // First entry will be zeroed in arch_setup
    .long page_table0
    .fill KERNEL_PAGE_OFFSET / (PAGE_SIZE * 1024) - 1, 4, 0
    .long page_table0 + 0 * PAGE_SIZE
    .long page_table0 + 1 * PAGE_SIZE
    .long page_table0 + 2 * PAGE_SIZE
    .long page_table0 + 3 * PAGE_SIZE
    .long page_table0 + 4 * PAGE_SIZE
    .long page_table0 + 5 * PAGE_SIZE
    .long page_table0 + 6 * PAGE_SIZE
    .long page_table0 + 7 * PAGE_SIZE
ALIGN(PAGE_SIZE)
END(kernel_page_dir)
