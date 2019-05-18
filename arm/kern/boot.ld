SECTIONS
{
	.text ALIGN(4096) :
	{
		_image_start = .;
		_boot_start = .;
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

	. = . + 256;
	
	. = ALIGN(4096);

	_ex_stack_top = .;
	_boot_end = .;
	
	.bundle :
	{
		kernel.bo(.bundle)
			. = ALIGN(4096);
		proc0.bo(.bundle)
		. = ALIGN(4096);
		bundle.bo(.bundle)
	}
	
  . = ALIGN(4096);
  _image_end = .;
}

