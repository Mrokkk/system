#include <arch/asm.h>
#include <arch/page.h>
#include <arch/segment.h>
#include <arch/register.h>
#include <arch/multiboot.h>
#include <arch/descriptor.h>

#include <kernel/segmexec.h>

#define FLAGS       MULTIBOOT_HEADER_FLAGS_ALIGNED
#define MAGIC       MULTIBOOT_HEADER_MAGIC
#define CHECKSUM    -(MAGIC + FLAGS)

.code32
SECTION(.data.multiboot)

ENTRY(multiboot_header)
    .long MAGIC
    .long FLAGS
    .long CHECKSUM
END(multiboot_header)

SECTION(.text.boot)

ENTRY(_start)

    // According to Multiboot specification, there's
    // a magic number in EAX and pointer to
    // multiboot_info structure in EBX

        // Zero EFLAGS register
        pushl $0
        popfl

        // Save multiboot data
        push %eax
        push %ebx

        call long_mode_check
        // Initialize page table to cover whole loaded memory
        mov $pgt1, %edi
        mov $PAGE_KERNEL_FLAGS, %eax

    1:
        cmp $_end - KERNEL_PAGE_OFFSET + PAGE_KERNEL_FLAGS, %eax
        jg 1f

        mov %eax, (%edi)
        add $8, %edi
        add $PAGE_SIZE, %eax
        jmp 1b

    1:
        mov $pgt3, %eax
        or $PAGE_KERNEL_FLAGS, %eax
        mov %eax, pgt4

        mov $pgt2, %eax
        or $PAGE_KERNEL_FLAGS, %eax
        mov %eax, pgt3

        mov $pgt1, %eax
        or $PAGE_KERNEL_FLAGS, %eax
        mov %eax, pgt2

        mov %cr4, %eax
        or $CR4_PAE, %eax
        mov %eax, %cr4

        mov $0xc0000080, %ecx
        rdmsr
        or $(1 << 8), %eax
        wrmsr

        // Load page directory
        mov $pgt4, %eax
        movl %eax, %cr3

        // Enable paging
        mov %cr0, %eax
        or $CR0_PE | CR0_PG | CR0_WP, %eax
        mov %eax, %cr0
        mov $1f, %eax
        jmp *%eax

    1:  // Load global descriptors table
        lgdt (gdt)

        // Set segment registers
        mov $KERNEL_DS, %eax
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs
        mov %ax, %ss

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
    1:  jmp 1b

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

long_mode:
    .code64
    1:
        mov $hello, %rsi
        call print64

    1:  hlt
        jmp 1b

print64:
        mov $0xb8000, %rdi
        mov $(7 << 8), %rbx
    1:
        cmpb $0, (%rsi)
        jz 1f
        movb (%rsi), %bl
        movw %bx, (%rdi)
        inc %rsi
        add $2, %rdi
        jmp 1b
    1:  ret

message:
    .string "CPU does not support long mode!\0"

hello:
    .string "Hello from 64 bit!\0"

ENDPROC(_start)

SECTION(.data.gdt)

ALIGN(32)
ENTRY(unpaged_gdt_entries)

    // Null segment
    descriptor_entry(0, 0, 0)

    // Kernel code segment
    descriptor_entry64(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE, 0, 0)

    // Kernel data segment
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_4KB | GDT_FLAGS_32BIT, 0, 0)

    // User code segment
    descriptor_entry(GDT_FLAGS_RING3 | GDT_FLAGS_TYPE_CODE, 0, 0)

    // User data segment
    descriptor_entry64(GDT_FLAGS_RING3 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_4KB | GDT_FLAGS_32BIT, 0, 0)

    // TSS
    descriptor_entry(GDT_FLAGS_TYPE_32TSS, 0, 0)

    // APM BIOS 32bit code
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_1B | GDT_FLAGS_32BIT, 0, 0xffff)

    // APM BIOS 16bit code
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_1B | GDT_FLAGS_16BIT, 0, 0xffff)

    // APM BIOS data
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_1B | GDT_FLAGS_32BIT, 0, 0xffff)

    // User TLS segment
    descriptor_entry(GDT_FLAGS_RING3 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_4KB | GDT_FLAGS_32BIT, 0, 0)

END(unpaged_gdt_entries)

ALIGN(32)
ENTRY(gdt)
    .word . - unpaged_gdt_entries - 1
    .quad unpaged_gdt_entries
END(gdt)

SECTION(.data.pagetable)
ALIGN(PAGE_SIZE)
ENTRY(pgt1)
    .fill 8 * PAGE_SIZE, 1, 0

ENTRY(pgt2)
    .fill PAGE_SIZE, 1, 0

ENTRY(pgt3)
    .fill PAGE_SIZE, 1, 0

ENTRY(pgt4)
    .fill PAGE_SIZE, 1, 0
