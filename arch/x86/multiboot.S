#include <arch/asm.h>
#include <arch/multiboot.h>

#define FLAGS       MULTIBOOT_HEADER_FLAGS_ALIGNED
#define MAGIC       MULTIBOOT_HEADER_MAGIC
#define CHECKSUM    -(MAGIC + FLAGS)

SECTION(.data.multiboot)

ENTRY(multiboot_header)
    .long MAGIC
    .long FLAGS
    .long CHECKSUM
END(multiboot_header)
