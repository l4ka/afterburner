/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/amd64/debug.cc
 * Description:   Debug support.
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
 * $Id: debug.cc,v 1.2 2005/11/14 16:10:23 joshua Exp $
 *
 ********************************************************************/

#include INC_ARCH(cpu.h)
#include INC_ARCH(debug.h)
#include <console.h>

void dump_gdt( dtr_t &gdt )
{
    printf( "gdt, base %p, limit %x:\n", gdt.x.fields.base,
	    gdt.x.fields.limit );
#if 0
    for( u32_t i = 0; i < gdt.get_total_desc(); i++ )
    {
	segdesc_t &seg = gdt.get_desc_table()[i];
	if( !seg.is_present() )
	    continue;
	printf( "%u: %s, base: %p, limit: %x\n", i, 
	        seg.is_data() ? "data":seg.is_code() ? "code":"system",
	        seg.get_base(), seg.get_limit()*seg.get_scale() );
    }
#endif
}

void dump_hardware_gdt()
{
    dtr_t gdt;
    __asm__ __volatile__ ("sgdt %0" : "=m"(gdt));
    printf( "hardware gdt, base %p, limit %x\n", gdt.x.fields.base,
	    gdt.x.fields.limit );
}

void dump_idt( dtr_t &idt )
{
    printf( "idt, base %p, limit %x:\n", idt.x.fields.base,
	    idt.x.fields.limit );
    for( u32_t i = 0; i < idt.get_total_gates(); i++ )
    {
	sysdesc_t &gate = idt.get_gate_table()[i];
	if( !gate.is_present() )
	    continue;
	printf( "%u: %s, offset %x, segment %u, dpl %u\n",
	        gate.is_trap_gate() ? "trap" : gate.is_int_gate() ? "interrupt"
		    : "unknown",
	        gate.as_gate()->get_offset(),
	        gate.as_gate()->get_segment_selector(),
	        gate.get_privilege() );
    }
}

void dump_hardware_idt()
{
    dtr_t idt;
    __asm__ __volatile__ ("sidt %0" : "=m"(idt));
    printf( "hardware idt, base %p, limit %x\n", idt.x.fields.base,
	    idt.x.fields.limit );
}

void dump_hardware_ldt()
{
    u16_t selector;
    __asm__ __volatile__ ("sldt %0" : "=r"(selector));
    printf( "hardware ldt segment selector: %u", selector );
}


