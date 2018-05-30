SECTIONS
{
	.text ALIGN(4096) :
	{
		_kernel_start = .;
		*(.text)
	}
	
	.rodata ALIGN(4096) : 
	{
		*(.rodata)
		*(.rodata.*)
	}

	.data ALIGN(4096) : 
	{
		*(.data)
		*(.data.*)
	}
	
	.bss ALIGN(4096) : 
	{
		_bss_start = .;
		*(.bss)
		*(.bss.*)
		_bss_end = .;
	}

  . = ALIGN(4096);

	. = . + 4096;
	
	_ex_stack_top = .;

  _kernel_end = .;
}

