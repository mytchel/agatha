SECTIONS
{
	. = 0xff000000;

	_kernel_start = .;

	.text ALIGN(4096) :
	{
		*(.text)
		*(.rodata)
	}

	.data ALIGN(4096) :
	{
		*(.data)
		*(.bss)

		_stack_start = .;
		. = . + 4096;
		. = ALIGN(4096);
		_stack_end = .;
	}

  _kernel_end = .;
}

