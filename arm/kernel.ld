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
		_kernel_bss_start = .;
		*(.bss)
		*(.bss.*)
		_kernel_bss_end = .;
	}

  . = ALIGN(4096);

	. = . + 4096;
	
	_ex_stack_top = .;
	
	.bundle :
	{
		. = ALIGN(4096);
		proc0.bo(.bundle)
		. = ALIGN(4096);
		bundle.bo(.bundle)
	}
	
  . = ALIGN(4096);
  _kernel_end = .;
}

