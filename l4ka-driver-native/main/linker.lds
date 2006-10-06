
ENTRY(c_runtime_init)

_start_vaddr = cfg_link_vaddr ;

SECTIONS
{
	.text SIZEOF_HEADERS + _start_vaddr :
	{
		*(.text)
		*(.gnu.linkonce.*)
	}

	.rodata . :
	{
		*(.rodata*)
		*(.ctors)
		*(.dtors)
	}

	. = ALIGN(4K);
	.data . :
	{
		*(.data*)
		*(.data.*)
		_bss_start = .;
		*(.bss)
		_bss_end = .;
		*(.stack.*)
	}

	. = ALIGN(1K);

	.hypervisor . :
	{
		*(.hypervisor)
	}

	.resourcemon.startup . :
	{
		*(.resourcemon.startup)
	}


	. = ALIGN(4K);
	.aligned . :
	{
		*(.aligned)
		*(.aligned.*)
	}

	.l4utcb . :
	{
		*(.l4utcb)
	}

	.l4kip . :
	{
		*(.l4kip)
	}

	_end = .;
	_end_vaddr = .;
}
