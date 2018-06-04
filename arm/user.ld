ENTRY(_start)

SECTIONS
{
	.text ALIGN(4096) :
	{
		_text_start = .;
		*sys.o(.text)
		*(.text)
		_text_end = .;
	}

	.data ALIGN(4096) :
	{
		_data_start = .;
		*(.data)
		*(.rodata)
		*(.bss)
		_data_end = .;
	}
}

