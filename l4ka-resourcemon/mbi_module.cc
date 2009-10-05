
/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     mbi_module.cc
 * Description:   Code for abstracting the MBI boot loader modules.
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

#include <debug.h>
#include <basics.h>
#include <mbi_module.h>

#define SKIP_MODULES	3

bool mbi_module_t::get_module_info( L4_Word_t index, const char* &cmdline, 
   	L4_Word_t &haddr_start, L4_Word_t &size )
{
    if( !this->mbi || (index >= this->get_total()) )
	return false;

    cmdline = this->mbi->mods[ index+SKIP_MODULES ].cmdline;
    haddr_start = this->mbi->mods[ index+SKIP_MODULES ].start;
    size = this->mbi->mods[ index+SKIP_MODULES ].end - haddr_start;

    return true;
}

L4_Word_t mbi_module_t::get_total()
{
    if( NULL == this->mbi )
	return 0;
    if( this->mbi->modcount < SKIP_MODULES )
	return 0;
    return this->mbi->modcount - SKIP_MODULES;
}

bool mbi_module_t::init()
{
    if( this->mbi )
	return true;
    this->locate_kickstart_turd();
    return this->mbi != NULL;
}

void mbi_module_t::locate_kickstart_turd()
{
    // Search for bootloader turds in the memory descriptors.
    L4_Word_t num_mdesc = L4_NumMemoryDescriptors( l4_kip );

    for( L4_Word_t idx = 0; idx < num_mdesc; idx++ )
    {
	L4_MemoryDesc_t *mdesc = L4_MemoryDesc( l4_kip, idx );
	L4_Word_t start = L4_MemoryDescLow(mdesc);
	L4_Word_t end   = L4_MemoryDescHigh(mdesc);

	if( !this->mbi && 
		(L4_MemoryDescType(mdesc) == bootloader_memdesc_init_table) )
	{
	    if( !sigma0_request_region(start, end) )
	    {
		printf( "Unable to request the MBI from sigma0.\n");
		return;
	    }
	    this->mbi = (mbi_t *)L4_MemoryDescLow(mdesc);
	    if( L4_BootInfo_Valid(this->mbi) )
		this->mbi = NULL;	// Not an MBI turd.
	    else
		printf("Mapped MBI %p - %p\n", start, end);
	}

	else if( L4_MemoryDescType(mdesc) == bootloader_memdesc_boot_module )
	{
	    if( !sigma0_request_region(start, end) )
	    {
		printf( "Unable to request an MBI module from sigma0.\n");
		return;
	    }
	    printf("Mapped MBI module %p - %p\n", start, end);
	}
    }
}

