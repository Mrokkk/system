#include <arch/asm.h>
#include <arch/smp.h>
#include <arch/register.h>
#include <arch/descriptor.h>
#include <arch/page_table.h>
#include <arch/page_types.h>

#define RELOCATED(name) (SMP_CODE_AREA + name - smp_entry)

.code16
ENTRY(smp_entry)
    cli
    cld
    ljmp $0, $RELOCATED(1f)

1:
    SEGMENT_REGISTERS_SET(0, ax)

    mov %edx, %esi
    mov $1, %eax
    cpuid
    shrl $24, %ebx
    mov %ebx, %eax

    mov $SMP_CPU_SIZE, %cl
    mul %cl
    add $SMP_SIGNATURE_AREA, %eax
    mov %eax, %edi

    lgdt (RELOCATED(smp_gdt))
    mov $CR0_PE, %eax
    mov %eax, %cr0

    ljmp $KERNEL_CS, $RELOCATED(1f)

ALIGN(16)
smp_gdt:
    .word smp_entry_end - smp_gdt_entries - 1
    .long RELOCATED(smp_gdt_entries)

ALIGN(16)
smp_gdt_entries:

    // Null segment
    descriptor_entry(0, 0, 0, 0)

    // Kernel code segment
    descriptor_entry(DESC_ACCESS_CODE | DESC_ACCESS_RING(0), DESC_FLAGS_4KB | DESC_FLAGS_32BIT, 0, 0xfffff)

    // Kernel data segment
    descriptor_entry(DESC_ACCESS_DATA | DESC_ACCESS_RING(0), DESC_FLAGS_4KB | DESC_FLAGS_32BIT, 0, 0xfffff)

smp_entry_end:
.global smp_entry_end

.code32
1:
    SEGMENT_REGISTERS_SET(KERNEL_DS, ax)

    mov SMP_CR3_OFFSET(%edi), %eax
    mov %eax, %cr3

    mov SMP_CR0_OFFSET(%edi), %eax
    mov %eax, %cr0

    mov SMP_CR4_OFFSET(%edi), %eax
    mov %eax, %cr4

    mov $1f, %eax
    jmp *%eax

1:
    mov SMP_STACK_OFFSET(%edi), %esp
    lidt (idt)
    lgdt (gdt)
    ljmp $KERNEL_CS, $1f

1:
    push %esi
    push %ebx
    push $0
    jmp smp_kmain

1:
    hlt
    jmp 1b

ENDPROC(smp_entry)
