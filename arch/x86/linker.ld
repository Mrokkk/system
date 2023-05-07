ENTRY(_start)

_kernel_phys = 0x00100000;
_kernel_virt = 0xC0100000;

_kernel_offset = _kernel_virt - _kernel_phys;

SECTIONS
{
    . = _kernel_phys;

    .unpaged_transient BLOCK(4K) : ALIGN(4K)
    {
        _unpaged_transient_start = .;
        KEEP(*(.data.multiboot))
        KEEP(*(.text.boot))
        KEEP(*(.data.pagetable))
    }

    .unpaged_eternal BLOCK(4K) : ALIGN(4K)
    {
        _unpaged_transient_end = .;
        _unpaged_eternal_start = .;
        KEEP(*(.data.gdt))
        KEEP(*(.data.pagedir))
        . = ALIGN(4K);
        _unpaged_eternal_end = .;
    }

    . += _kernel_offset;

    .text_init ALIGN(4K) : AT(ADDR(.text_init) - _kernel_offset)
    {
        _text_init_start = .;
        KEEP(*(.text.init))
    }

    .text ALIGN(4K) : AT(ADDR(.text) - _kernel_offset)
    {
        _text_init_end = .;
        _text_start = .;
        *(.text)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - _kernel_offset)
    {
        _text_end = .;
        _rodata_start = .;
        *(.rodata)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - _kernel_offset)
    {
        _rodata_end = .;
        _data_start = .;
        *(.data)
        _smodules_data = .;
        KEEP(*(.modules_data))
        _emodules_data = .;
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - _kernel_offset)
    {
        _data_end = .;
        _bss_start = _data_end;
        *(COMMON)
        *(.bss)
    }

    . = ALIGN(4K);

    _bss_end = .;
    _end = .;
}
