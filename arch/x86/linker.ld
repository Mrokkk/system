ENTRY(_start)

SECTIONS
{

	. = 1M;

	.text BLOCK(4K) : ALIGN(4K)
	{
		_stext = .;
		KEEP(*(.multiboot))
		KEEP(*(.text.boot))
		*(.text)
		_etext = .;
	}

	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	}

	.data BLOCK(4K) : ALIGN(4K)
	{
		_sdata = .;
		*(.data)
		_smodules_data = .;
		KEEP(*(.modules_data))
		_emodules_data = .;
		_edata = .;
	}	

	.bss BLOCK(4K) : ALIGN(4K)
	{
		_sbss = _edata;
		*(COMMON)
		*(.bss)
		*(.stack)
		_ebss = .;
	}
	
	. = ALIGN(4K);
	
	_end = .;
	
}
