OUTPUT_FORMAT ("elf32-littlearm")
ENTRY(_start)

SECTIONS
{

    . = 0x8000;
    
    .text : ALIGN(4K)
    {
    	_stext = .;
        KEEP(*(.text.boot))
        *(.text)
        _etext = .;
    }
 
    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }
 
    .data : ALIGN(4K)
    {
        _sdata = .;
        *(.data)
        _smodules_data = .;
        KEEP(*(.modules_data))
        _emodules_data = .;
        _edata = .;
    }

    .bss (NOLOAD) : ALIGN(4K)
    {
    	_sbss = .;
    	*(COMMON)
        *(.bss)
        *(.stack)
        _ebss = .;
    }
    
    . = ALIGN(4K);
    
    _end = .;
    
}
