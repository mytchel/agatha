ENTRY(_start)

SECTIONS
{
	.text ALIGN(4096):
	{
    _text_start = .;
		*(.text)
		*(.text.*)
		*(.rodata)
		*(.rodata.*)
	}
	
	. = ALIGN(4096);
	_text_end = .;
	
	_data_start = .;
	.data :
	{
		*(.data)
		*(.data.*)
		*(.bss)
		*(.bss.*)
		
	}
	
	. = ALIGN(4096);
	_data_end = .;
}

