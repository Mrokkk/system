ENTRY(_start)

_kernel_phys = 0x00100000;
_kernel_virt = 0xC0100000;

_kernel_offset = _kernel_virt - _kernel_phys;

SECTIONS {

    . = _kernel_phys;

    .unpagged BLOCK(4K) : ALIGN(4K) {
        KEEP(*(.multiboot))
        KEEP(*(.text.boot))
    }

    . += _kernel_offset;

    . = ALIGN(4K);
    .text BLOCK(4K) : AT(ADDR(.text) - _kernel_offset) {
        _stext = .;
        *(.text)
        _etext = .;
    }

    . = ALIGN(4K);
    .rodata BLOCK(4K) : AT(ADDR(.rodata) - _kernel_offset) {
        *(.rodata)
    }

    . = ALIGN(4K);
    .data BLOCK(4K) : AT(ADDR(.data) - _kernel_offset) {
        _sdata = .;
        *(.data)
        _smodules_data = .;
        KEEP(*(.modules_data))
        _emodules_data = .;
        _edata = .;
    }

    . = ALIGN(4K);
    .bss BLOCK(4K) : AT(ADDR(.bss) - _kernel_offset) {
        _sbss = _edata;
        *(COMMON)
        *(.bss)
        *(.stack)
        _ebss = .;
    }

    . = ALIGN(4K);

    _end = .;

}

