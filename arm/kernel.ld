SECTIONS
{
	.text ALIGN(4096) :
	{
		_kernel_start = .;
		*(.text)
		*(.rodata)
	}

	.data ALIGN(4096) : 
	{
		*(.data)
		_bss_start = .;
		*(.bss)
		_bss_end = .;
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

