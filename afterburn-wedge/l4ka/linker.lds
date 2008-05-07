/*********************************************************************
 *                
 * Copyright (C) 1999-2006, 2008,  Karlsruhe University
 *                
 * File path:     linker.lds
 * Description:   L4Ka linker script for x86.
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
 * $Id: linker.lds,v 1.9 2005/11/12 11:38:18 joshua Exp $
 *                
 ********************************************************************/

ENTRY(afterburn_c_runtime_init)

_start_wedge = CONFIG_WEDGE_VIRT;

#if defined(CONFIG_VMI_LINKER_SCRIPT)
#include "../ia32/vmi.lds"
#else
#define SECTION_VMI
#endif

#include INC_WEDGE(config.h)

SECTIONS
{
	.text _start_wedge :
	{
		*(.text*)
		*(.spinlock)
		*(.gnu.linkonce.*)

		. = ALIGN(16);

		/* cpu-local static initializers */
                __ctors_CPU__ = .;
                *(SORT(.ctors.3*))
                QUAD(0)

		/* node-local static initializers */
                __ctors_NODE__ = .;
                *(SORT(.ctors.2*))
                QUAD(0)

		/* global static initializers */
                __ctors_GLOBAL__ = .;
		*(SORT(.ctors.1*))
		*(SORT(.ctors.*))
                *(.ctors)
                QUAD(0)

		*(.dtors)
	}

	.rodata . :
	{
		*(.rodata*)
	}
	
	SECTION_VMI

	. = ALIGN(4K);
	_start_vcpulocal = .;
	.vcpulocal . :
	{
		*(.vcpulocal.vcpu)
		*(.vcpulocal.*)
		. = ALIGN(4K);
	}
	_sizeof_vcpulocal = SIZEOF(.vcpulocal);
	_end_vcpulocal = .;
	_start_vcpulocal_shadow = .;
	. = . + (SIZEOF(.vcpulocal) * (CONFIG_NR_VCPUS));
	_end_vcpulocal_shadow = .;

	. = ALIGN(CONFIG_KIP_AREA_SIZE);
	.l4kip . : { *(.l4kip*) }
	. = ALIGN(CONFIG_UTCB_AREA_SIZE);
	.l4utcb . : { *(.l4utcb*) }
	.resourcemon . : { *(.resourcemon) }
	.resourcemon.startup . : { *(.resourcemon.startup*) }
	.burn_symbols . : { *(.burn_symbols) }
	.burn_symbol_strings . : { *(.burn_symbol_strings) }
	.data . : {
		*(.data*)
		_start_bss = .;
		*(.bss)
		_end_bss = .;

	}
#if defined(CONFIG_L4KA_HVM)
	. = ALIGN(4K);
	.rombios : {
		rombios/rombios.elf(.bios)
	}
	.vgabios : {
		vgabios/vgabios.elf(.bios)
	}
#endif
	_end_wedge = .;

#if defined(CONFIG_DEBUG_SYMBOLS)
	/* Stabs debugging sections.  */
	.stab          0 : { *(.stab) }
	.stabstr       0 : { *(.stabstr) }
	.stab.excl     0 : { *(.stab.excl) }
	.stab.exclstr  0 : { *(.stab.exclstr) }
	.stab.index    0 : { *(.stab.index) }
	.stab.indexstr 0 : { *(.stab.indexstr) }
	.comment       0 : { *(.comment) }
	/* DWARF debug sections.
	   Symbols in the DWARF debugging sections are relative to the beginning
	   of the section so we begin them at 0.  */
	/* DWARF 1 */
	.debug          0 : { *(.debug) }
	.line           0 : { *(.line) }
	/* GNU DWARF 1 extensions */
	.debug_srcinfo  0 : { *(.debug_srcinfo) }
	.debug_sfnames  0 : { *(.debug_sfnames) }
	/* DWARF 1.1 and DWARF 2 */
	.debug_aranges  0 : { *(.debug_aranges) }
	.debug_pubnames 0 : { *(.debug_pubnames) }
	/* DWARF 2 */
	.debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
	.debug_abbrev   0 : { *(.debug_abbrev) }
	.debug_line     0 : { *(.debug_line) }
	.debug_frame    0 : { *(.debug_frame) }
	.debug_str      0 : { *(.debug_str) }
	.debug_loc      0 : { *(.debug_loc) }
	.debug_macinfo  0 : { *(.debug_macinfo) }
	/* SGI/MIPS DWARF 2 extensions */
	.debug_weaknames 0 : { *(.debug_weaknames) }
	.debug_funcnames 0 : { *(.debug_funcnames) }
	.debug_typenames 0 : { *(.debug_typenames) }
	.debug_varnames  0 : { *(.debug_varnames) }

#endif

}
