/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     l4ka-driver-native/main/linker.lds
 * Description:   
 *                
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *                
 * $Id$
 *                
 ********************************************************************/

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

	.resourcemon . :
	{
		*(.resourcemon)
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
