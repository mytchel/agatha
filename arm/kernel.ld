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

		_stack_start = .;
		. = . + 256;
		. = ALIGN(4096);
		_stack_end = .;
	}

  _kernel_end = .;
}

