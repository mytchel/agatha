
SECTIONS
{
	. = ALIGN(4096);
	_text_start = .;
	.text :
	{
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
  
  _self_end = .;
}
