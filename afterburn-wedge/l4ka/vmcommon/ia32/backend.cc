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

#include INC_ARCH(page.h)
#include INC_ARCH(cpu.h)
#include INC_ARCH(intlogic.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(resourcemon.h)
#include INC_WEDGE(dspace.h)
#include <device/acpi.h>
#include <device/apic.h>
#include <device/dp83820.h>

#include INC_WEDGE(message.h)

#include <burn_counters.h>
static const bool debug_vector=0;
static const bool debug_user_access=0;
static const bool debug_page_not_present=1;

INLINE bool async_safe( word_t ip )
{
    return ip < CONFIG_WEDGE_VIRT;
}



pgent_t *
backend_resolve_addr( word_t user_vaddr, word_t &kernel_vaddr )
{
    vcpu_t &vcpu = get_vcpu();
    word_t kernel_start = vcpu.get_kernel_vaddr();

    L4_Word_t wedge_addr = vcpu.get_wedge_vaddr();
    L4_Word_t wedge_end_addr = vcpu.get_wedge_end_vaddr();
    
    static pgent_t wedge_pgent;
    if( (user_vaddr >= wedge_addr) && (user_vaddr < wedge_end_addr) )
    {
	wedge_pgent.set_address(user_vaddr 
		- vcpu.get_wedge_vaddr() 
		+ vcpu.get_wedge_paddr());
	return &wedge_pgent;
    }
    
    pgent_t *pdir = (pgent_t *)(vcpu.cpu.cr3.get_pdir_addr() + kernel_start);
    pdir = &pdir[ pgent_t::get_pdir_idx(user_vaddr) ];
    if( !pdir->is_valid() )
	return NULL;
    if( EXPECT_FALSE(pdir->is_superpage()) ) {
	kernel_vaddr = (pdir->get_address() & SUPERPAGE_MASK)
	    + (user_vaddr & ~SUPERPAGE_MASK) + kernel_start;
	return pdir;
    }

    pgent_t *ptab = (pgent_t *)(pdir->get_address() + kernel_start);
    pgent_t *pgent = &ptab[ pgent_t::get_ptab_idx(user_vaddr) ];
    if( !pgent->is_valid() )
	return NULL;
    kernel_vaddr = pgent->get_address() + (user_vaddr & ~PAGE_MASK) 
	+ kernel_start;

    return pgent;
}

void backend_enable_paging( word_t *ret_address )
{
    vcpu_t &vcpu = get_vcpu();
    CORBA_Environment ipc_env = idl4_default_environment;
    bool int_save;


    // Reconfigure our virtual address window in the VMM.
    int_save = vcpu.cpu.disable_interrupts();
    IResourcemon_set_virtual_offset( resourcemon_shared.thread_server_tid, 
	    vcpu.get_kernel_vaddr(), &ipc_env );
    vcpu.cpu.restore_interrupts( int_save );

    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &ipc_env );
	panic();
    }

    // Convert the return address into a virtual address.
    // TODO: make this work with static stuff too.  Currently it depends
    // on the burn.S code.
    *ret_address += vcpu.get_kernel_vaddr();

    // Flush our current mappings.
    backend_flush_user();
}


static bool deliver_ia32_vector( 
    cpu_t & cpu, L4_Word_t vector, u32_t error_code, thread_info_t *thread_info)
{
    // - Byte offset from beginning of IDT base is 8*vector.
    // - Compare the offset to the limit value.  The limit is expressed in 
    // bytes, and added to the base to get the address of the last
    // valid byte.
    // - An empty descriptor slot should have its present flag set to 0.

    ASSERT( L4_MyLocalId() == get_vcpu().monitor_ltid );

    if( vector > cpu.idtr.get_total_gates() ) {
	con << "No IDT entry for vector " << vector << '\n';
	return false;
    }

    gate_t *idt = cpu.idtr.get_gate_table();
    gate_t &gate = idt[ vector ];

    ASSERT( gate.is_trap() || gate.is_interrupt() );
    ASSERT( gate.is_present() );
    ASSERT( gate.is_32bit() );

    if(debug_vector)
	con << "Delivering vector " << vector
	    << ", handler ip " << (void *)gate.get_offset()
	    << "\n";

    u16_t old_cs = cpu.cs;
    L4_Word_t old_eip, old_esp, old_eflags;
    
#if !defined(CONFIG_L4KA_VMEXT)
    L4_Word_t dummy;
    L4_ThreadId_t dummy_tid, res;
    L4_ThreadId_t main_ltid = get_vcpu().main_ltid;

    // Read registers of page fault.
    // TODO: get rid of this call to exchgregs ... perhaps enhance page fault
    // protocol with more info.
    res = L4_ExchangeRegisters( main_ltid, 0, 0, 0, 0, 0, L4_nilthread,
	    (L4_Word_t *)&dummy, 
	    (L4_Word_t *)&old_esp, 
	    (L4_Word_t *)&old_eip, 
	    (L4_Word_t *)&old_eflags, 
	    (L4_Word_t *)&dummy, &dummy_tid );
    if( L4_IsNilThread(res) )
	return false;
#else
    old_esp = thread_info->mr_save.get(OFS_MR_SAVE_ESP);
    old_eip = thread_info->mr_save.get(OFS_MR_SAVE_EIP); 
    old_eflags = thread_info->mr_save.get(OFS_MR_SAVE_EFLAGS); 
#endif

    if( !async_safe(old_eip) )
    {
	con << "interrupting the wedge to handle a fault,"
	    << " ip " << (void *) old_eip
	    << " vector " << vector
	    << ", cr2 "  << (void *) cpu.cr2
	    << ", handler ip "  << (void *) gate.get_offset() 
	    << ", called from " << (void *) __builtin_return_address(0) 
	    << "\n";
	DEBUGGER_ENTER("BUG");
    }
    
    // Set VCPU flags
    cpu.flags.x.raw = (old_eflags & flags_user_mask) | (cpu.flags.x.raw & ~flags_user_mask);
    
    // Store values on the stack.
    L4_Word_t *esp = (L4_Word_t *) old_esp;
    *(--esp) = cpu.flags.x.raw;
    *(--esp) = old_cs;
    *(--esp) = old_eip;
    *(--esp) = error_code;

    cpu.cs = gate.get_segment_selector();
    cpu.flags.prepare_for_gate( gate );

#if !defined(CONFIG_L4KA_VMEXT)
    // Update thread registers with target execution point.
    res = L4_ExchangeRegisters( main_ltid, 3 << 3 /* i,s */,
				(L4_Word_t) esp, gate.get_offset(), 0, 0, L4_nilthread,
				&dummy, &dummy, &dummy, &dummy, &dummy, &dummy_tid );
    if( L4_IsNilThread(res) )
	return false;
#else
    thread_info->mr_save.set(OFS_MR_SAVE_ESP, (L4_Word_t) esp);
    thread_info->mr_save.set(OFS_MR_SAVE_EIP, gate.get_offset());
#endif
    
    return true;
}

word_t vcpu_t::get_map_addr(word_t fault_addr)
{
    return fault_addr;
}

bool vcpu_t::handle_wedge_pfault(thread_info_t *ti, map_info_t &map_info, bool &nilmapping)
{
#if defined(CONFIG_WEDGE_STATIC)
    return false;
#endif
    word_t fault_addr = ti->mr_save.get_pfault_addr();
    word_t fault_ip = ti->mr_save.get_pfault_ip();
    
    map_info.rwx = 7;
    
    const word_t wedge_addr = get_wedge_vaddr();
    const word_t wedge_end_addr = get_wedge_end_vaddr();
    idl4_fpage_t fp;
    CORBA_Environment ipc_env = idl4_default_environment;
    
    if ((fault_addr >= wedge_addr) && (fault_addr < wedge_end_addr))
    {
	// A page fault in the wedge.
	map_info.addr = fault_addr;
	
	if( debug_pfault )
	    con << "Wedge Page fault\n";
	word_t map_vcpu_id = cpu_id;
#if defined(CONFIG_VSMP)
	if (is_booting_other_vcpu())
	{
	    if (debug_startup)
	    {
		con << "bootstrap AP monitor, send wedge page "
		    << (void *) map_info.addr  << "\n";
	    }
	    
	    // Make sure we have write access t the page
	    * (volatile word_t *) map_info.addr = * (volatile word_t *) map_info.addr;
	    map_vcpu_id = get_booted_cpu_id();
	    nilmapping = false;
	}
	else 
#endif
	{
	    idl4_set_rcv_window( &ipc_env, L4_CompleteAddressSpace );
	    IResourcemon_pagefault( L4_Pager(), map_info.addr, fault_ip, map_info.rwx, &fp, &ipc_env);
	    nilmapping = true;
	}	

#if defined(CONFIG_SMP_ONE_AS)
	ASSERT(!IS_VCPULOCAL(fault_addr));
#else
	if (IS_VCPULOCAL(fault_addr))
	    map_info.addr = (word_t) get_on_vcpu((word_t *) fault_addr, map_vcpu_id);
#endif
	return true;
    } 

    return false;

}

bool vcpu_t::resolve_paddr(thread_info_t *ti, map_info_t &map_info, word_t &paddr, bool &nilmapping)
{
    ASSERT(ti);
    word_t fault_addr = ti->mr_save.get_pfault_addr();
    word_t fault_ip = ti->mr_save.get_pfault_ip();
    word_t fault_rwx = ti->mr_save.get_pfault_rwx();
    
    word_t link_addr = get_kernel_vaddr();

    paddr = fault_addr;
    if( cpu.cr0.paging_enabled() )
    {
	pgent_t *pdir = (pgent_t *)(cpu.cr3.get_pdir_addr() + link_addr);
	pdir = &pdir[ pgent_t::get_pdir_idx(fault_addr) ];
	if( !pdir->is_valid() )
	{
	    extern pgent_t *guest_pdir_master;
	    if( EXPECT_FALSE((ti->get_tid() != main_gtid) && guest_pdir_master) )
	    {
		// We are on a support L4 thread, running from a kernel
		// module.  Since we have no valid context for delivering
		// a page fault to the guest, we'll instead try looking
		// in the guest's master pdir for the ptab.
		pdir = &guest_pdir_master[ pgent_t::get_pdir_idx(fault_addr) ];
	    }
	    if( !pdir->is_valid() ) 
	       	goto not_present;
	}

	if( pdir->is_superpage() && cpu.cr4.is_pse_enabled() ) {
	    paddr = (pdir->get_address() & SUPERPAGE_MASK) + 
		(fault_addr & ~SUPERPAGE_MASK);
	    map_info.page_bits = SUPERPAGE_BITS;
	    if( !pdir->is_writable() )
		map_info.rwx = 5;
	    if( debug_superpages )
		con << "super page fault at " << (void *)fault_addr << '\n';
	    if( debug_user_access && 
		    !pdir->is_kernel() && (fault_addr < link_addr) )
	       	con << "user access, fault_ip " << (void *)fault_ip << '\n';

	    vaddr_stats_update( fault_addr & SUPERPAGE_MASK, pdir->is_global());
	}
	else 
	{
	    pgent_t *ptab = (pgent_t *)(pdir->get_address() + link_addr);
	    pgent_t *pgent = &ptab[ pgent_t::get_ptab_idx(fault_addr) ];
	    if( !pgent->is_valid() )
		goto not_present;
	    paddr = pgent->get_address() + (fault_addr & ~PAGE_MASK);
	    if( !pgent->is_writable() )
		map_info.rwx = 5;
	    if( debug_user_access &&
		    !pgent->is_kernel() && (fault_addr < link_addr) )
	       	con << "user access, fault_ip " << (void *)fault_ip << '\n';

	    vaddr_stats_update( fault_addr & PAGE_MASK, pgent->is_global());
	}

	if( ((map_info.rwx & fault_rwx) != fault_rwx) && cpu.cr0.write_protect() )
	    goto permissions_fault;
    }
    else if( paddr > link_addr )
	paddr -= link_addr;
    
    return false;
    
 not_present:
    if( debug_page_not_present )
    {
	con << "page not present, fault addr " << (void *)fault_addr
	    << ", ip " << (void *)fault_ip << '\n';
    }
    if( ti->get_tid() != main_gtid )
	PANIC( "fatal page fault (page not present) in L4 thread %x, ti %x address %x, ip %x", 
		ti->get_tid(), ti, fault_addr, fault_ip);
    cpu.cr2 = fault_addr;
    if( deliver_ia32_vector(cpu, 14, (fault_rwx & 2) | 0, ti )) {
	map_info.addr = fault_addr;
	nilmapping = true; 
	return true;
    }
    goto unhandled;

 permissions_fault:
    if (debug_pfault)
	con << "Delivering page fault for addr " << (void *)fault_addr
	    << ", permissions " << fault_rwx 
	    << ", ip " << (void *) fault_ip 
	    << ", tid " << ti->get_tid()
	    << "\n";    
    if( ti->get_tid() != main_gtid )
	PANIC( "Fatal page fault (permissions) in L4 thread %x, ti %x, address %x, ip %x,",
		ti->get_tid().raw, ti, fault_addr, fault_ip);
    cpu.cr2 = fault_addr;
    if( deliver_ia32_vector(cpu, 14, (fault_rwx & 2) | 1, ti)) {
	map_info.addr = fault_addr;
	nilmapping = true; 
	return true;
    }
    /* fall through */

 unhandled:
    con << "Unhandled page permissions fault, fault addr " << (void *)fault_addr
	<< ", ip " << (void *)fault_ip << ", fault rwx " << fault_rwx << '\n';
    panic();
   


    
}
