/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:	resourcemon/pager.cc
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
 ********************************************************************/

#include <l4/kip.h>
#include <l4/thread.h>
#include <l4/message.h>
#include <l4/ipc.h>
#include <l4/sigma0.h>

#include <common/debug.h>
#include <common/basics.h>
#include <resourcemon/resourcemon.h>
#include "resourcemon_idl_server.h"
#include <resourcemon/vm.h>
#include <common/console.h>


static bool debug_device_request = false;
static bool debug_fake_device_request = false;

/* Device remapping region, use a 32MB window */
static const L4_Word_t dev_remap_szlog2 = (5 + 10 + 10);
static const L4_Word_t dev_remap_size = (1UL << dev_remap_szlog2);  
static L4_Fpage_t dev_remap_area = L4_Nilpage;

/* Remap table based on entries of the following size */
static const L4_Word_t dev_remap_pgsizelog2 = (12);
static const L4_Word_t dev_remap_pgsize = (1UL << dev_remap_pgsizelog2);
static const L4_Word_t dev_remap_tblsize = dev_remap_size / dev_remap_pgsize;

/* Remap table based on entries of the following size */
static union {
    L4_Word_t raw;
    struct {
	L4_Word_t ref	:12;
	L4_Word_t phys	:20;
    } __attribute__((packed)) x;
} dev_remap_table[dev_remap_tblsize];    

static L4_Word_t dev_remap_tblmaxused = 0;


static L4_Word_t last_virtual_byte = 0;

static L4_KernelInterfacePage_t *kip = NULL;

/*
 * We assume all non-conventional memory to be device memory
 */

static bool is_device_mem(vm_t *vm, L4_Word_t low, L4_Word_t high)
{
    
    for( L4_Word_t d=0; d<IResourcemon_max_devices;d++ )
    {
	word_t vmlow = vm->get_device_low(d);
	word_t vmhigh = vm->get_device_high(d);
	
	if (vmlow == vmhigh)
	    return false;
	
	if ((vmlow  >= low && vmlow  <  high) ||
	    (vmhigh >  low && vmhigh <= high) ||
	    (vmlow  <= low && vmhigh >= high))

	    return true;
    }
    return false;
	    
}



#if defined(cfg_vga_passthrough)
extern inline bool is_vga( L4_Word_t addr )
{
    return (addr >= 0xa0000) && (addr < 0xf0000);
}
#else
extern inline bool is_vga( L4_Word_t addr )
{
    return false;
}
#endif



IDL4_INLINE void IResourcemon_register_pdirptr_implementation(
    	CORBA_Object _caller ATTR_UNUSED_PARAM, 
       	const L4_Word_t addr ATTR_UNUSED_PARAM, 
	idl4_server_environment *_env)
{
    L4_KDB_Enter( "register pdirptr" );

    idl4_set_no_response( _env );
}
IDL4_PUBLISH_IRESOURCEMON_REGISTER_PDIRPTR(IResourcemon_register_pdirptr_implementation);

IDL4_INLINE void IResourcemon_set_virtual_offset_implementation(
	CORBA_Object _caller,
	const L4_Word_t new_offset,
	idl4_server_environment *_env)
{
    vm_t *vm = get_vm_allocator()->tid_to_vm( _caller );
    if( !vm )
    {
	hprintf( 0, PREFIX "page fault for invalid client.\n" );
	idl4_set_no_response( _env );
	return;
    }

    vm->set_vaddr_offset( new_offset );
}
IDL4_PUBLISH_IRESOURCEMON_SET_VIRTUAL_OFFSET(IResourcemon_set_virtual_offset_implementation);


IDL4_INLINE void IResourcemon_pagefault_implementation(
    CORBA_Object _caller,
    const L4_Word_t pf_addr,
    const L4_Word_t ip,
    const L4_Word_t privileges,
    idl4_fpage_t *fp,
    idl4_server_environment *_env)

{
    vm_t *vm = get_vm_allocator()->tid_to_vm( _caller );
    if( !vm )
    {
	hprintf( 0, PREFIX "page fault for invalid client.\n" );
	idl4_set_no_response( _env );
	return;
    }

    L4_Word_t addr = pf_addr, haddr, paddr;

    hprintf( 5, PREFIX "page fault, space %lu, ip %p, addr %p\n", 
	     vm->get_space_id(), (void *)ip, (void *)addr );


    if( is_vga(addr) ) // XXX: for direct accesses at the physical
		       // address (l4-io), this is broken, Josh!
	haddr = addr;
    else if (vm->client_vaddr_to_paddr(addr, &paddr) && is_vga(paddr))
	haddr = paddr;
    else if( !vm->client_vaddr_to_haddr(addr, &haddr) )
    {
	hprintf( 0, PREFIX "page fault for invalid address, space %lu, ip %p, "
		 "addr %p, access %lx, halting ...\n", 
		 vm->get_space_id(), (void *)ip, (void *)addr, privileges );
	hprintf( 0, PREFIX "VM's addr space size: %lx\n", vm->get_space_size());
	idl4_set_no_response( _env );
	L4_KDB_Enter("VM panic");
	return;
    }
    
    word_t low = haddr, high = haddr + 4095;
    if(is_device_mem(vm, low, high) || addr == 0x800c0000)
    {
	// Don't give out mappings for the client's kip or utcb.
	CORBA_exception_set( _env, ex_IResourcemon_invalid_mem_region, NULL );
	hout << "Client requested device mem via paging"
	     << " req " << (void *) low
	     << " end " << (void *) high
	     << "\n";
	return;
    }
    // Ensure that we have the page.
    *(volatile char *)haddr;

    idl4_fpage_set_base( fp, addr );
    idl4_fpage_set_page( fp, L4_FpageLog2( haddr, PAGE_BITS) );
    idl4_fpage_set_mode( fp, IDL4_MODE_MAP );
    idl4_fpage_set_permissions( fp, privileges );
}
IDL4_PUBLISH_IRESOURCEMON_PAGEFAULT(IResourcemon_pagefault_implementation);


IDL4_INLINE void IResourcemon_request_pages_implementation(
    CORBA_Object _caller,
    const L4_Word_t req_fp_raw,
    const L4_Word_t attr ATTR_UNUSED_PARAM,
    idl4_fpage_t *fp,
    idl4_server_environment *_env)
{
    L4_Fpage_t req_fp( (L4_Fpage_t){ raw: req_fp_raw } );

    vm_t *vm = get_vm_allocator()->tid_to_vm( _caller );
    if( !vm )
    {
	hprintf( 0, PREFIX "page request for invalid client.\n" );
	idl4_set_no_response( _env );
	return;
    }

    idl4_fpage_set_page( fp, L4_Nilpage );
    idl4_fpage_set_base( fp, 0 );
    idl4_fpage_set_mode( fp, IDL4_MODE_MAP );
    idl4_fpage_set_permissions( fp,
				IDL4_PERM_WRITE | IDL4_PERM_READ | IDL4_PERM_EXECUTE );

#if defined(cfg_small_pages)
#warning Using small pages!!
    req_fp = L4_FpageLog2( L4_Address(req_fp), PAGE_BITS );
#endif

    if( is_fpage_intersect(vm->get_kip_fp(), req_fp) ||
	is_fpage_intersect(vm->get_utcb_fp(), req_fp) )
    {
	// Don't give out mappings for the client's kip or utcb.
	CORBA_exception_set( _env, ex_IResourcemon_invalid_mem_region, NULL );
	hout << "Client requested a kip or utcb alias.\n";
	return;
    }

    L4_Word_t paddr = L4_Address(req_fp);
    L4_Word_t paddr_end = paddr + L4_Size(req_fp) - 1;
    L4_Word_t haddr, haddr2;
    if( is_vga(paddr) && is_vga(paddr_end) ) {
	idl4_fpage_set_base( fp, L4_Address(req_fp) );
	idl4_fpage_set_page( fp, L4_FpageLog2(paddr, L4_SizeLog2(req_fp)) );
    }
    else if( vm->client_paddr_to_haddr(paddr, &haddr) &&
	     vm->client_paddr_to_haddr(paddr_end, &haddr2) )
    {
	idl4_fpage_set_base( fp, L4_Address(req_fp) );
	idl4_fpage_set_page( fp, L4_FpageLog2(haddr, L4_SizeLog2(req_fp)) );
    }
    else {
	CORBA_exception_set( _env, ex_IResourcemon_invalid_mem_region, NULL );
	hout << "pager cannot return " << (void*) paddr << " @ " << (void *) haddr << "\n";
    }
    
    L4_Word_t req = haddr;
    L4_Word_t req_end = haddr + L4_Size(req_fp) - 1;
    if(is_device_mem(vm, req, req_end))
    {
	// Don't give out mappings for the client's kip or utcb.
	CORBA_exception_set( _env, ex_IResourcemon_invalid_mem_region, NULL );
	hout << "Client requested device mem via paging"
	     << " req " << (void *) req	
	     << " end " << (void *) req_end
	     << "\n";
	return;
    }

    
}
IDL4_PUBLISH_IRESOURCEMON_REQUEST_PAGES(IResourcemon_request_pages_implementation);


IDL4_INLINE void IResourcemon_request_device_implementation(
    CORBA_Object _caller,
    const L4_Word_t req_fp_raw,
    const L4_Word_t attr ATTR_UNUSED_PARAM,
    idl4_fpage_t *fp,
    idl4_server_environment *_env)

{
    L4_Fpage_t req_fp( (L4_Fpage_t){ raw: req_fp_raw } );
    req_fp += L4_FullyAccessible;

    vm_t *vm = get_vm_allocator()->tid_to_vm( _caller );
    if( !vm )
    {
	hprintf( 0, PREFIX "device request from invalid client.\n" );
	idl4_set_no_response( _env );
	return;
    }

    idl4_fpage_set_page( fp, L4_Nilpage );
    idl4_fpage_set_base( fp, L4_Address(req_fp) );
    
    if (L4_IsNilFpage(req_fp))
	return;

    L4_Word_t req = L4_Address(req_fp);
    L4_Word_t req_end = req + L4_Size(req_fp) - 1;

    if (L4_Size(req_fp) > dev_remap_pgsize)
    {
	hout << "Device mappings larger than " << dev_remap_pgsize << " not implemented\n";  
	L4_KDB_Enter("unimplemented");
    }

    if(!is_device_mem(vm, req, req_end))
    {
	if (debug_fake_device_request)
	    hprintf( 1, PREFIX "fake device request, space %lx, addr %p, "
		     "size %lu\n", vm->get_space_id(), (void *)L4_Address(req_fp),
		     L4_Size(req_fp) );
	// We respond with the guest physical memory here
	//L4_KDB_Enter("Here");
 	L4_Word_t req_haddr, req_haddr_end;
 	if( !(vm->client_paddr_to_haddr(req, &req_haddr)) ||
 	    !(vm->client_paddr_to_haddr(req_end , &req_haddr_end)))
 	{
 	    hout << "Couldn't determine haddr for fake device mem page\n";
 	    return;
 	}

	L4_Fpage_t req_haddr_fp = L4_FpageLog2(req_haddr, L4_SizeLog2(req_fp)) + L4_FullyAccessible;
	idl4_fpage_set_mode( fp, IDL4_MODE_MAP );
	idl4_fpage_set_page( fp, req_haddr_fp );
	idl4_fpage_set_base( fp, L4_Address(req_haddr_fp) );
	idl4_fpage_set_permissions( fp,
				    IDL4_PERM_WRITE | IDL4_PERM_READ | IDL4_PERM_EXECUTE );
	return;
    }

    if (debug_device_request)
	hout << "device request, space " << (void *) vm->get_space_id()
	     << " addr " << (void *)L4_Address(req_fp)
	     << " size " << L4_Size(req_fp)
	     << "\n";

	
    /* Do not hand out requests larger than dev_remap_pgsize at once */
    bool found = false;
    L4_Word_t window_idx = 0;
    L4_Word_t req_aligned = (L4_Address(req_fp) & ~(dev_remap_pgsize-1));
	    
    /* Search device remap table */
    for (L4_Word_t idx=0; idx <= dev_remap_tblmaxused; idx++)
    {
	if (dev_remap_table[idx].x.ref > 0 &&
	    dev_remap_table[idx].x.phys == (req_aligned >> 12))
	{
	    if (debug_device_request)
		hout << "Found mapping in remap table, entry" << idx << "\n";
	    window_idx = idx;
	    found = true;
	    break;
	}
    }
	    
    if (!found)
    {
	    
	window_idx = dev_remap_tblsize;
		
	for (L4_Word_t idx=0; idx <= dev_remap_tblsize; idx++)
	    if (dev_remap_table[idx].x.ref == 0)
	    {
		window_idx = idx;
		break;
	    }
	    
	if (window_idx == dev_remap_tblsize)
	{
	    hout << "Device remap table size full, consider increasing dev_remap_tblsize\n";
	    L4_KDB_Enter("Device remapping failed");
	    return;
	}
		
	if (dev_remap_tblmaxused < window_idx)
	    dev_remap_tblmaxused = window_idx;
		
	L4_Fpage_t remap_window;
	    
	if (debug_device_request)
	    hout << "Establishing mapping in remap table "
		 << "entry " << window_idx  
		 << " req " << (void *) req_aligned << "\n";
		
		
	L4_Fpage_t sigma0_rcv;
	L4_Fpage_t dev_phys;

	// Request  mapping from Sigma0
	remap_window = L4_FpageLog2(
	    L4_Address(dev_remap_area) + (window_idx * dev_remap_pgsize),
	    dev_remap_pgsizelog2);
		
	dev_phys = L4_FpageLog2(req_aligned, dev_remap_pgsizelog2) + L4_FullyAccessible;
		
	sigma0_rcv = L4_Sigma0_GetPage( L4_nilthread, dev_phys, remap_window );
		
	if( L4_IsNilFpage(sigma0_rcv) || (L4_Rights(sigma0_rcv) != L4_FullyAccessible))
	{
	    hout << "device request got nilmapping from s0"
		 << ", space " << vm->get_space_id()
		 << ", addr " << (void *)L4_Address(dev_phys) 
		 << ", size " << L4_Size(dev_phys) 
		 << ", req_addr " << (void *) req 
		 << ", req_size " << L4_Size(req_fp) 
		 << ", fill with dummy mempage (MEMFAKE)\n";
		    
	    /* 
	     * Fill unsuccessful request with other mem pages.
	     * Remap the guest's haddr into the window
	     * This is a HACK, since a second guest gets that
	     * memory, too. But then again, this is true for device
	     * memory anyways.
	     */
		    
	    L4_Word_t req_haddr, req_haddr_end;
		    
	    if( !(vm->client_paddr_to_haddr(req, &req_haddr)) ||
		!(vm->client_paddr_to_haddr(req_end , &req_haddr_end)))
	    {
		hout << "Couldn't determine haddr for fake device mem page\n";
		return;
	    }
		    
	    dev_phys = L4_FpageLog2(req_haddr, dev_remap_pgsizelog2) + L4_FullyAccessible;
	    sigma0_rcv = L4_Sigma0_GetPage( L4_nilthread, dev_phys, remap_window);
		    
	    if (L4_IsNilFpage(sigma0_rcv))
	    {
		hout << "device request got nilmapping from s0 (MEMFAKE),"
		     << " space " << vm->get_space_id()
		     << " addr " << (void *)L4_Address(dev_phys) 
		     << " size " << L4_Size(dev_phys)
		     << ", req_addr " << (void *)L4_Address(req_fp) 
		     << ", req_size " << L4_Size(req_fp) 
		     << "\n";
		return;
			
	    }
	}	    
	dev_remap_table[window_idx].x.phys = (req_aligned >> 12);
    }
    dev_remap_table[window_idx].x.ref += 1;
	    
	    
#warning VU: not SMP safe
    L4_Word_t remap_addr = L4_Address(dev_remap_area) + (window_idx * dev_remap_pgsize) 
	+ ((L4_Address(req_fp) & (dev_remap_pgsize - 1)));
	    
    L4_Fpage_t remap_fp = L4_FpageLog2(remap_addr, L4_SizeLog2(req_fp));  

    // Pass the mapping to the client.
    idl4_fpage_set_mode( fp, IDL4_MODE_MAP );
    idl4_fpage_set_page( fp, remap_fp );
    idl4_fpage_set_permissions( fp,
				IDL4_PERM_WRITE | IDL4_PERM_READ | IDL4_PERM_EXECUTE );
	    

}

IDL4_PUBLISH_IRESOURCEMON_REQUEST_DEVICE(IResourcemon_request_device_implementation);

inline void IResourcemon_unmap_device_implementation(CORBA_Object _caller, 
						    const L4_Word_t req_fp_raw, 
						    const L4_Word_t attr, 
						    L4_Word_t *old_attr, 
						    idl4_server_environment *_env)
{

    L4_Fpage_t req_fp( (L4_Fpage_t){ raw: req_fp_raw } );
    req_fp += L4_FullyAccessible;
    
    *old_attr = 0;
    
    vm_t *vm = get_vm_allocator()->tid_to_vm( _caller );
    if( !vm )
    {
	    hprintf( 0, PREFIX "device request from invalid client.\n" );
	idl4_set_no_response( _env );
	return;
    }

    if (L4_IsNilFpage(req_fp))
    {
	CORBA_exception_set( _env, ex_IResourcemon_device_not_mapped, NULL );
	return;
    }

    /* Do not hand out requests larger than dev_remap_pgsize at once */
    if (L4_Size(req_fp) > dev_remap_pgsize)
    {
	hout << "Device unmappings larger than " << dev_remap_pgsize << " unimplemented\n";
	L4_KDB_Enter("unimplemented");
    }

    
    L4_Word_t req = L4_Address(req_fp);
    L4_Word_t req_end = L4_Address(req_fp) + L4_Size(req_fp) - 1;
    
    if(!is_device_mem(vm, req, req_end))
    {
	if (debug_fake_device_request)
	    hprintf( 1, PREFIX "fake device unmap request, space %lx, fp %p\n", 
		     vm->get_space_id(), req_fp.raw );
	
	L4_Word_t req_haddr, req_haddr_end;
 	if( !(vm->client_paddr_to_haddr(req, &req_haddr)) ||
 	    !(vm->client_paddr_to_haddr(req_end , &req_haddr_end)))
 	{
 	    hout << "Couldn't determine haddr for fake device mem page\n";
 	    return;
 	}

	L4_Fpage_t req_haddr_fp = L4_FpageLog2(req_haddr, L4_SizeLog2(req_fp)) + L4_FullyAccessible;
	L4_UnmapFpage(req_haddr_fp);
	return;
    }
    if (debug_device_request)
	hout << "device unmap request, space " << (void *) vm->get_space_id()
	     << " addr " << (void *)L4_Address(req_fp)
	     << " size " << L4_Size(req_fp)
	     << "\n";

    L4_Word_t window_idx = dev_remap_tblsize;
    L4_Word_t req_aligned = (L4_Address(req_fp) & ~(dev_remap_pgsize-1));

    for (L4_Word_t idx=0; idx <= dev_remap_tblmaxused; idx++)
    {
	if (dev_remap_table[idx].x.phys == (req_aligned >> 12) 
	    && dev_remap_table[idx].x.ref != 0)
	{
	    if (debug_device_request)
 		hout << "Found mapping in remap table"
		     << " entry " << idx 
		     << " refcount " << dev_remap_table[idx].x.ref << "\n";
	    window_idx = idx;
	    break;
	}
    }
    
    if (window_idx == dev_remap_tblsize)
    {
	if (debug_device_request)
	    hout << "Did not find device mapping in remap table"
		 << " addr " << (void *)L4_Address(req_fp)
		 << " size " << L4_Size(req_fp)
		 << "\n";
	return;
    }
    
    if (window_idx == dev_remap_tblmaxused)
	for (L4_Word_t idx=dev_remap_tblmaxused; idx >= 0; idx--)
	    if (dev_remap_table[idx].x.ref != 0)
	    {
		dev_remap_tblmaxused = idx;
		break;
	    }
    
    if (attr != L4_FullyAccessible)
    {
	L4_Fpage_t window_fp = L4_FpageLog2(
	    L4_Address(dev_remap_area) + (window_idx * dev_remap_pgsize),
	    dev_remap_pgsizelog2) + attr;

	if (debug_device_request)
	    hout << "Only unmap partly / check refbits " << (void *) window_fp.raw << "\n";

	L4_Fpage_t old_attr_fp = L4_UnmapFpage(window_fp);
	*old_attr = L4_Rights(old_attr_fp);
    }
    else if (--dev_remap_table[window_idx].x.ref == 0)
    {
	L4_Fpage_t window_fp = L4_FpageLog2(
	    L4_Address(dev_remap_area) + (window_idx * dev_remap_pgsize),
	    dev_remap_pgsizelog2) + L4_FullyAccessible;
	
	if (debug_device_request)
	    hout << "Refcount zero, flush whole window " << (void *) window_fp.raw << "\n";

	dev_remap_table[window_idx].x.phys = 0;
	L4_Fpage_t old_attr_fp = L4_Flush(window_fp);
	*old_attr = L4_Rights(old_attr_fp);
	
    } else {
	if (debug_device_request)
	    hout << "Refcount nonzero, unmap requested fpage\n";
	L4_Word_t window_addr = L4_Address(dev_remap_area) + (window_idx * dev_remap_pgsize) 
	    + ((L4_Address(req_fp) & (dev_remap_pgsize - 1)));
	
	L4_Fpage_t window_fp = L4_FpageLog2(window_addr, L4_SizeLog2(req_fp)) + L4_FullyAccessible;  
	
	L4_Fpage_t old_attr_fp = L4_UnmapFpage(window_fp);
	*old_attr = L4_Rights(old_attr_fp);
    }	    
    

}
IDL4_PUBLISH_IRESOURCEMON_UNMAP_DEVICE(IResourcemon_unmap_device_implementation);



IDL4_INLINE void IResourcemon_request_client_pages_implementation(
	CORBA_Object _caller,
	const L4_ThreadId_t *client_tid,
	const L4_Word_t req_fp_raw,
	idl4_fpage_t *fp,
	idl4_server_environment *_env)
{
    L4_Fpage_t req_fp( (L4_Fpage_t){ raw: req_fp_raw } );

    vm_t *vm = get_vm_allocator()->tid_to_vm( _caller );
    if( !vm ) {
	hout << "request from invalid client.\n";
	idl4_set_no_response( _env );
	return;
    }

    idl4_fpage_set_page( fp, L4_Nilpage );
    idl4_fpage_set_base( fp, L4_Address(req_fp) );
    
    if( L4_IsNilFpage(req_fp) )
	return;
    if( !vm->has_device_access() ) {
	hout << "VM lacks sufficient privileges\n";
	CORBA_exception_set( _env, ex_IResourcemon_no_permission, NULL );
	return;
    }

    vm_t *client_vm = get_vm_allocator()->tid_to_vm( *client_tid );
    if( !client_vm ) {
	hout << "Invalid client VM tid.\n";
	CORBA_exception_set( _env, ex_IResourcemon_unknown_client, NULL );
	return;
    }

    L4_Word_t req_end = L4_Address(req_fp) + L4_Size(req_fp) - 1;
    if( req_end > client_vm->get_space_size() ) {
	hout << "Client pages request exceeds size of client VM.\n";
	CORBA_exception_set( _env, ex_IResourcemon_invalid_mem_region, NULL );
	return;
    }

    L4_Fpage_t send_fp = L4_FpageLog2( 
	    client_vm->get_haddr_base() + L4_Address(req_fp),
	    L4_SizeLog2(req_fp) );
    idl4_fpage_set_page( fp, send_fp );
    idl4_fpage_set_mode( fp, IDL4_MODE_MAP );
    idl4_fpage_set_permissions( fp, 
	    IDL4_PERM_READ | IDL4_PERM_WRITE | IDL4_PERM_EXECUTE );

    hout << "A DD/OS is mapping a client page range, client addr "
	 << (void *)L4_Address(req_fp)
	 << ", size " << L4_Size(req_fp) 
	 << " to " << (void *)L4_Address(send_fp) << '\n';
}
IDL4_PUBLISH_IRESOURCEMON_REQUEST_CLIENT_PAGES(IResourcemon_request_client_pages_implementation);


IDL4_INLINE void IResourcemon_get_client_phys_range_implementation(
	CORBA_Object _caller,
	const L4_ThreadId_t *client_tid,
	L4_Word_t *phys_start,
	L4_Word_t *phys_size,
	idl4_server_environment *_env)
{
    vm_t *vm = get_vm_allocator()->tid_to_vm( _caller );
    if( !vm )
    {
	hprintf( 0, PREFIX "DMA request from invalid client.\n" );
	idl4_set_no_response( _env );
	return;
    }

    vm = get_vm_allocator()->tid_to_vm( *client_tid );
    if( !vm )
    {
	CORBA_exception_set( _env, ex_IResourcemon_unknown_client, NULL );
	return;
    }

    *phys_start = vm->get_haddr_base();
    *phys_size = vm->get_space_size();
}
IDL4_PUBLISH_IRESOURCEMON_GET_CLIENT_PHYS_RANGE(IResourcemon_get_client_phys_range_implementation);

IDL4_INLINE void IResourcemon_get_space_phys_range_implementation(
	CORBA_Object _caller,
	const L4_Word_t space_id,
	L4_Word_t *phys_start,
	L4_Word_t *phys_size,
	idl4_server_environment *_env)
{
    vm_t *vm = get_vm_allocator()->tid_to_vm( _caller );
    if( !vm )
    {
	hprintf( 0, PREFIX "DMA request from invalid client.\n" );
	idl4_set_no_response( _env );
	return;
    }

    vm = get_vm_allocator()->space_id_to_vm( space_id );
    if( !vm )
    {
	CORBA_exception_set( _env, ex_IResourcemon_unknown_client, NULL );
	return;
    }

    *phys_start = vm->get_haddr_base();
    *phys_size = vm->get_space_size();
}
IDL4_PUBLISH_IRESOURCEMON_GET_SPACE_PHYS_RANGE(IResourcemon_get_space_phys_range_implementation);


void pager_init( void )
{
    // Find a window in our virtual address space which will never 
    // overlap with useable physical memory, to use for device mappings.
    // We can use the Pistachio kernel physical memory area.

    kip = (L4_KernelInterfacePage_t *)L4_GetKernelInterface();

    L4_Word_t num_mdesc = L4_NumMemoryDescriptors( kip );

    /* First search for end of virtual address space */
    for( L4_Word_t idx = 0; idx < num_mdesc; idx++ )
    {
	L4_MemoryDesc_t *mdesc = L4_MemoryDesc( kip, idx );
	L4_Word_t end = L4_MemoryDescHigh(mdesc);

	if ( L4_IsVirtual(mdesc) && 
	     L4_MemoryDescType(mdesc) == L4_ConventionalMemoryType )
	    if (last_virtual_byte < end)
		last_virtual_byte = end;
    }

    /* Hardcode to 3GB if search failed */
    if ( last_virtual_byte == 0 )
	last_virtual_byte = 0xc0000000 - 1;

    /* 
     * Now search for virtual address region that is not backed by
     * conventional or archspecific region. If not found, resort to 
     * a region backed by conventional memory 
     */
    
    bool redo_conventional = false;
    
redo:   
    for ( L4_Word_t dev_remap_end = ( (last_virtual_byte + 1) & ~(dev_remap_size-1));
	  dev_remap_end != dev_remap_size; dev_remap_end -= dev_remap_size )
    {
	
	L4_Word_t dev_remap_start = dev_remap_end - dev_remap_size;
	
	hout << "Try " << (void *) dev_remap_start << " - " 
	     << (void *) (dev_remap_end) << "\n";
	
	L4_Fpage_t fp = L4_Fpage( dev_remap_start, dev_remap_size );
	bool overlap = false;
	
	for( L4_Word_t idx = 0; idx < num_mdesc; idx++ )
	{
	    L4_MemoryDesc_t *mdesc = L4_MemoryDesc( kip, idx );
	    L4_Word_t start = L4_MemoryDescLow(mdesc);
	    L4_Word_t end = L4_MemoryDescHigh(mdesc);
	    
	    overlap = overlap || 
		(!L4_IsVirtual(mdesc) && 
		 (redo_conventional ? (L4_MemoryDescType(mdesc) != L4_ConventionalMemoryType) : true) &&
		 (L4_MemoryDescType(mdesc) != L4_SharedMemoryType) &&
		 (L4_MemoryDescType(mdesc) != L4_ReservedMemoryType) &&
		 (start < dev_remap_end && end >= dev_remap_start));
	    

	    if (!L4_IsVirtual(mdesc) && 
		(L4_MemoryDescType(mdesc) != L4_SharedMemoryType) &&
		(L4_MemoryDescType(mdesc) != L4_ReservedMemoryType) &&
		(start < dev_remap_end && end >= dev_remap_start))
		hout << "\toverlaps with " << (void *) start << " - " << (void*) end 
		     << " type " << L4_MemoryDescType(mdesc) << "\n";
	    
	    
	}
	
	if (!overlap)
	{
	    dev_remap_area = fp;
	    break;
	}
    }

    // Did we find a suitable region?
    if( L4_SizeLog2(dev_remap_area) == 0 )
    {
	if (!redo_conventional)
	{
	    redo_conventional = true;
	    goto redo;
	}
	
	hout << "Did not find suitable device remap region";
	L4_KDB_Enter("No device remap area");
	dev_remap_area = L4_FpageLog2( 0x80000000, PAGE_BITS );
    }

    
    for (L4_Word_t idx=0; idx < dev_remap_tblsize; idx++)
	dev_remap_table[idx].raw = 0;
    hout << "size of dev remap table " << sizeof(dev_remap_table) << "\n";
    
    
    hout << "Device remap region: " << (void *)L4_Address(dev_remap_area)
	 << ", size " << (void *)L4_Size(dev_remap_area) << '\n';
    hout << "Virtual address space end: " << (void*)last_virtual_byte << '\n';
}
