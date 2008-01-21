/*********************************************************************
 *                
 * Copyright (C) 1999-2005,  Karlsruhe University   vim:ft=ld
 *                
 * File path:     afterburn-wedge/kaxen/linker.lds
 * Description:   KaXen linker script for amd64.
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
 ********************************************************************/

ENTRY(kaxen_wedge_start)

#include INC_ARCH(page.h)

_start_wedge = CONFIG_WEDGE_VIRT;

SECTIONS
{
	. = 0;
	__xen_guest : 
	{
		*(.low__xen_guest)
	}

	.low :
	{
		*(.low*)
	}

	. += _start_wedge;

	.text : AT(ADDR(.text) - _start_wedge)
	{
		*(.text)
		*(.gnu.linkonce.*)
	}

	.rodata : AT(ADDR(.rodata) - _start_wedge)
	{
		*(.rodata*)

		/* XXX temporary!! */
		. = ALIGN(4K);
		xen_shared_info = .;
		. += 4K;
	}
	
	. = ALIGN(4K);
	.data : AT(ADDR(.data) - _start_wedge)
	{
		*(.data)
		_bss_start = .;
		*(.bss)
		_bss_end = .;
	}

	_end_wedge = .;
}

/* make sure tmp_region is sufficiently near to our data etc so we don't
   get a chicken and egg problem when mapping it */
tmp_region = ALIGN(4K);
mapping_base_region = tmp_region + CONFIG_WEDGE_TMP_REGION;
xen_p2m_region = mapping_base_region + 4K;
_wedge_real_end = xen_p2m_region + CONFIG_WEDGE_P2M_REGION;

