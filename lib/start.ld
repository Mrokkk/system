ENTRY(_start)

__start = 0x00001000;

SECTIONS
{
    . = __start;
    .text : { *(.text) }

    . = ALIGN(4K);
    .data : { *(.data) }
    .bss : {
        _sbss = .;
        *(.bss)
        _ebss = .;
    }
}
