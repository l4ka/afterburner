/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4ka/ia32/backend.cc
 * Description:   L4 backend implementation.  Specific to IA32 and to the
 *                research resource monitor.
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
#include INC_WEDGE(vm.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(debug.h)

word_t vcpu_t::get_map_addr(word_t fault_addr)
{
    //TODO: prevent overlapping
    if( fault_addr >= 0xbc000000 ) 
	return fault_addr - 0xbc000000 + 0x40000000;
	return fault_addr;
}

pgent_t *
backend_resolve_addr( word_t user_vaddr, word_t &kernel_vaddr )
{
    UNIMPLEMENTED();
}

bool vcpu_t::handle_wedge_pfault(thread_info_t *ti, map_info_t &map_info, bool &nilmapping)
{
    const word_t wedge_paddr = get_wedge_paddr();
    const word_t wedge_end_paddr = get_wedge_end_paddr();
    
    if ((map_info.addr >= wedge_paddr) && (map_info.addr < wedge_end_paddr))
    {
	con << "Wedge (Phys) pfault"
	    << ", subsitute with scratch page " << (void*) get_vm()->wedge_gphys << "\n";;
	map_info.addr = get_vm()->wedge_gphys;
	
    }
	
}

bool vcpu_t::resolve_paddr(thread_info_t *ti, map_info_t &map_info, word_t &paddr, bool &nilmapping)
{
    paddr = map_info.addr;
    return false;
}
