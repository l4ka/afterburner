/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/l4bootinfo_module.cc
 * Description:   Code for abstracting the L4 bootinfo modules.
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

#include <l4/kip.h>
#include <l4/bootinfo.h>

#include <common/debug.h>
#include <common/l4bootinfo_module.h>


bool l4bootinfo_module_t::get_module_info( L4_Word_t index,
	const char* &cmdline, L4_Word_t &haddr_start, L4_Word_t &size )
{
    if( !this->bootinfo || (index >= this->get_total()) )
	return false;

    L4_BootRec_t *rec = this->get_module(index);

    cmdline = L4_Module_Cmdline( rec );
    haddr_start = L4_Module_Start( rec );
    size = L4_Module_Size( rec );

    return true;
}

L4_Word_t l4bootinfo_module_t::get_total()
{
    return this->total;
}

bool l4bootinfo_module_t::init()
{
    if( this->bootinfo )
	return true;

    this->locate_kickstart_turd();
    if( this->bootinfo )
    {
	this->total = 0;
	L4_BootRec_t *rec = L4_BootInfo_FirstEntry( this->bootinfo );
	for( L4_Word_t i = 0; i < L4_BootInfo_Entries(this->bootinfo); i++ )
	{
	    if( L4_BootRec_Type(rec) == L4_BootInfo_Module )
		this->total++;
	    rec = L4_Next(rec);
	}
	return true;
    }

    return false;
}

L4_BootRec_t * l4bootinfo_module_t::get_module( L4_Word_t index )
    // Index must be valid!!
{
    L4_Word_t which = 0;
    L4_BootRec_t *rec = L4_BootInfo_FirstEntry( this->bootinfo );

    while( 1 )
    {
	if( L4_BootRec_Type(rec) == L4_BootInfo_Module )
	{
	    if( which == index )
		return rec;
	    which++;
	}
	rec = L4_Next(rec);
    }
}

void l4bootinfo_module_t::locate_kickstart_turd()
{
    L4_KernelInterfacePage_t *kip =
	(L4_KernelInterfacePage_t *)L4_GetKernelInterface();

    // Search for bootloader turds in the memory descriptors.
    L4_Word_t num_mdesc = L4_NumMemoryDescriptors( kip );

    for( L4_Word_t idx = 0; idx < num_mdesc; idx++ )
    {
	L4_MemoryDesc_t *mdesc = L4_MemoryDesc( kip, idx );
	L4_Word_t start = L4_MemoryDescLow(mdesc);
	L4_Word_t end   = L4_MemoryDescHigh(mdesc);

	if( !this->bootinfo && 
		(L4_MemoryDescType(mdesc) == bootloader_memdesc_init_table) )
	{
	    if( !sigma0_request_region(start, end) )
	    {
		printf( "Unable to request the bootinfo region from sigma0.\n");
		return;
	    }
	    this->bootinfo = (void *)L4_MemoryDescLow(mdesc);
	    if( L4_BootInfo_Valid(this->bootinfo) )
		dprintf(debug_startup,  "Mapped bootinfo region %p-%p\n", start, end);
	    else
		this->bootinfo = NULL;
	}

	else if( L4_MemoryDescType(mdesc) == bootloader_memdesc_boot_module )
	{
	    if( !sigma0_request_region(start, end) )
	    {
		printf( "Unable to request a bootloader module from sigma0.\n");
		return;
	    }
	    dprintf(debug_startup,  "Mapped bootloader module region %p-%p\n", start, end);
	}
    }
}

