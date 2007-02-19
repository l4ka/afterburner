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

static const bool debug=0;
static const bool debug_page_not_present=1;
static const bool debug_irq_forward=0;
static const bool debug_irq_deliver=0;
static const bool debug_pfault=0;
static const bool debug_superpages=0;
static const bool debug_user_access=0;
static const bool debug_dma=0;
static const bool debug_apic=0;
static const bool debug_device=0;
static const bool debug_startup=0;

DECLARE_BURN_COUNTER(async_delivery_canceled);

dspace_handlers_t dspace_handlers;

extern inline bool is_passthrough_mem( L4_Word_t addr )
{
#if defined(CONFIG_DEVICE_PASSTHRU) 
    return true;
#endif
#if defined(CONFIG_DEVICE_APIC)
    if (acpi.is_acpi_table(addr))
	return true;
#endif
    return addr <= (1024 * 1024);
}

INLINE bool async_safe( word_t ip )
{
    return ip < CONFIG_WEDGE_VIRT;
}

void backend_enable_paging( word_t *ret_address )
{
    vcpu_t &vcpu = get_vcpu();
    CORBA_Environment ipc_env = idl4_default_environment;
    bool int_save;


    // Reconfigure our virtual address window in the VMM.
    int_save = vcpu.cpu.disable_interrupts();
    IResourcemon_set_virtual_offset( 
	    resourcemon_shared.cpu[L4_ProcessorNo()].thread_server_tid,
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

    if(debug)
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
	DEBUGGER_ENTER(0);
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



thread_info_t * backend_handle_pagefault( L4_MsgTag_t tag, L4_ThreadId_t tid )
{
    vcpu_t &vcpu = get_vcpu();
    cpu_t &cpu = vcpu.cpu;

    thread_info_t *ti = NULL;
    word_t fault_addr = 0, fault_ip = 0, fault_rwx = 0, paddr = 0;
    word_t dev_req_page_size = 0; 

    word_t link_addr = vcpu.get_kernel_vaddr();
    const word_t wedge_addr = vcpu.get_wedge_vaddr();
    const word_t wedge_end_addr = vcpu.get_wedge_end_vaddr();

    word_t map_addr = 0;
    L4_MapItem_t map_item;
    word_t map_rwx = 7, map_page_bits = PAGE_BITS;
    bool nilmapping = false;

    idl4_fpage_t fp;
    CORBA_Environment ipc_env = idl4_default_environment;
	
    if( L4_UntypedWords(tag) != 2 ) {
	con << "Bogus page fault message from TID " << tid << '\n';
	return NULL;
    }
    
    if (tid == vcpu.main_gtid)
	ti = &vcpu.main_info;
    else if (tid == vcpu.irq_gtid)
	ti = &vcpu.irq_info;
#if defined(CONFIG_VSMP)
    else if (vcpu.is_booting_other_vcpu()
	    && tid == get_vcpu(vcpu.get_booted_cpu_id()).monitor_gtid)
	ti = &get_vcpu(vcpu.get_booted_cpu_id()).monitor_info;
#endif
    else if (vcpu.is_vcpu_hthread(tid))
	ti = &vcpu.hthread_info;
    else
    {
	mr_save_t tmp;
	tmp.store_mrs(tag);
	
	con << "invalid pfault message, VCPU " << vcpu.cpu_id  
	    << " addr: " << (void *) tmp.get_pfault_addr()
	    << ", ip: " << (void *) tmp.get_pfault_ip()
	    << ", rwx: " << (void *)  tmp.get_pfault_rwx()
	    << ", TID: " << tid 
	    << "\n";
	return NULL;
    }
    
    ti->mr_save.store_mrs(tag);
    
    if (debug_pfault)
    { 
	con << "pfault, VCPU " << vcpu.cpu_id  
	    << " addr: " << (void *) ti->mr_save.get_pfault_addr()
	    << ", ip: " << (void *) ti->mr_save.get_pfault_ip()
	    << ", rwx: " << (void *)  ti->mr_save.get_pfault_rwx()
	    << ", TID: " << tid << '\n'; 
    }  

    fault_addr = ti->mr_save.get_pfault_addr();
    fault_ip = ti->mr_save.get_pfault_ip();
    fault_rwx = ti->mr_save.get_pfault_rwx();
    paddr = fault_addr;

#if defined(CONFIG_DEVICE_DP83820) 
    dp83820_t *dp83820 = dp83820_t::get_pfault_device(ti->mr_save.get_pfault_addr());
    if( dp83820 ) 
    {
	dp83820->backend.handle_pfault( ti->mr_save.get_pfault_addr() );
	nilmapping = true;
	goto done;
    }
#endif
#if !defined(CONFIG_WEDGE_STATIC)
    if( (fault_addr >= wedge_addr) && (fault_addr < wedge_end_addr) )
    {
	// A page fault in the wedge.
	map_addr = fault_addr;
	
	if( debug_pfault )
	    con << "Wedge Page fault, " << (void *)fault_addr
		<< ", ip " << (void *)fault_ip << ", rwx " << fault_rwx << '\n';


	word_t map_vcpu_id = vcpu.cpu_id;
#if defined(CONFIG_VSMP)
	if (vcpu.is_booting_other_vcpu())
	{
	    if (debug_startup)
	    {
		con << "bootstrap AP monitor, send wedge page "
		    << (void *) map_addr 
		    << " (" << map_page_bits << ")"
		    << "\n";
	    }
	    
	    // Make sure we have write access t the page
	    * (volatile word_t *) map_addr = * (volatile word_t *) map_addr;
	    map_vcpu_id = vcpu.get_booted_cpu_id();
	    nilmapping = false;
	}
	else 
#endif
	{
	    idl4_set_rcv_window( &ipc_env, L4_CompleteAddressSpace );
	    IResourcemon_pagefault( L4_Pager(), map_addr, fault_ip, map_rwx, &fp, &ipc_env);
	    nilmapping = true;
	}	
	
	if (IS_VCPULOCAL(fault_addr))
	    map_addr = (word_t) GET_ON_VCPU(map_vcpu_id, word_t, fault_addr);

	goto done;
    } 
#endif
	
	    
    if( debug_pfault )
	con << "Page fault, " << (void *)fault_addr
	    << ", ip " << (void *)fault_ip << ", rwx " << fault_rwx << '\n';

    L4_Fpage_t map_fp;
    if( dspace_handlers.handle_pfault(fault_addr, &fault_ip, &map_fp) )
    {
	// Note: we ignore changes to ip.
	map_addr = L4_Address( map_fp );
	map_page_bits = L4_SizeLog2( map_fp );
	map_rwx = L4_Rights( map_fp );

	con << "DSpace Page fault, " << (void *)fault_addr
	    << ", ip " << (void *)fault_ip << ", rwx " << fault_rwx << '\n';

	nilmapping = (MASK_BITS(fault_addr, map_page_bits) == MASK_BITS(map_addr, map_page_bits));
	goto done;
    }
    
    if( cpu.cr0.paging_enabled() )
    {
	pgent_t *pdir = (pgent_t *)(cpu.cr3.get_pdir_addr() + link_addr);
	pdir = &pdir[ pgent_t::get_pdir_idx(fault_addr) ];
	if( !pdir->is_valid() )
	{
	    extern pgent_t *guest_pdir_master;
	    if( EXPECT_FALSE((tid != vcpu.main_gtid) && guest_pdir_master) )
	    {
		// We are on a support L4 thread, running from a kernel
		// module.  Since we have no valid context for delivering
		// a page fault to the guest, we'll instead try looking
		// in the guest's master pdir for the ptab.
		pdir = &guest_pdir_master[ pgent_t::get_pdir_idx(fault_addr) ];
	    }
	    if( !pdir->is_valid() ) {
	        if( debug_page_not_present )
		{
	    	    con << "pdir not present " << (void *) pdir << "\n";
		    DEBUGGER_ENTER(0);
		}
	       	goto not_present;
	    }
	}

	if( pdir->is_superpage() && cpu.cr4.is_pse_enabled() ) {
	    paddr = (pdir->get_address() & SUPERPAGE_MASK) + 
		(fault_addr & ~SUPERPAGE_MASK);
	    map_page_bits = SUPERPAGE_BITS;
	    if( !pdir->is_writable() )
		map_rwx = 5;
	    if( debug_superpages )
		con << "super page fault at " << (void *)fault_addr << '\n';
	    if( debug_user_access && 
		    !pdir->is_kernel() && (fault_addr < link_addr) )
	       	con << "user access, fault_ip " << (void *)fault_ip << '\n';

	    vcpu.vaddr_stats_update( fault_addr & SUPERPAGE_MASK, pdir->is_global());
	}
	else 
	{
	    pgent_t *ptab = (pgent_t *)(pdir->get_address() + link_addr);
	    pgent_t *pgent = &ptab[ pgent_t::get_ptab_idx(fault_addr) ];
	    if( !pgent->is_valid() )
		goto not_present;
	    paddr = pgent->get_address() + (fault_addr & ~PAGE_MASK);
	    if( !pgent->is_writable() )
		map_rwx = 5;
	    if( debug_user_access &&
		    !pgent->is_kernel() && (fault_addr < link_addr) )
	       	con << "user access, fault_ip " << (void *)fault_ip << '\n';

	    vcpu.vaddr_stats_update( fault_addr & PAGE_MASK, pgent->is_global());
	}

	if( ((map_rwx & fault_rwx) != fault_rwx) && cpu.cr0.write_protect() )
	    goto permissions_fault;
    }
    else if( paddr > link_addr )
	paddr -= link_addr;
 
    L4_Fpage_t fp_recv, fp_req;
#if defined(CONFIG_DEVICE_APIC) 
    word_t new_vaddr;
    if (acpi.is_virtual_acpi_table(paddr, new_vaddr))
    {
	if (debug_acpi)
	    con << "ACPI table override " 
		<< (void *) paddr << " -> " << (void *) new_vaddr << "\n";
	word_t new_paddr = new_vaddr - get_vcpu().get_wedge_vaddr() + get_vcpu().get_wedge_paddr();
	map_addr = paddr + link_addr;
	fp_recv = L4_FpageLog2( map_addr, PAGE_BITS );
    	fp_req = L4_FpageLog2(new_paddr, PAGE_BITS );
	idl4_set_rcv_window( &ipc_env, fp_recv );
	IResourcemon_request_pages( L4_Pager(), fp_req.raw, 7, &fp, &ipc_env );
	nilmapping = false;
	goto done;
    }    
#endif

#if 1
    // Only get a page, no matter if PT entry is a superpage
    dev_req_page_size = PAGE_SIZE;
#else 
    // Get the whole superpage for devices
    dev_req_page_size = (1UL << page_bits);
#endif

    if (contains_device_mem(paddr, paddr + (dev_req_page_size - 1)))
    {
	if (debug_device)
	    con << "device access, vaddr " << (void *)fault_addr
		<< ", map_addr " << (void *)map_addr
		<< ", paddr " << (void *)paddr 
		<< ", size "  << dev_req_page_size
		<< ", ip " << (void *)fault_ip 
		<< '\n';
	
	if (is_passthrough_mem(paddr))
	{	
	    map_addr = fault_addr & ~(dev_req_page_size -1);	
	    paddr &= ~(dev_req_page_size -1);
	    for (word_t pt=0; pt < dev_req_page_size ; pt+= PAGE_SIZE)
	    {
		fp_recv = L4_FpageLog2( map_addr + pt, PAGE_BITS );
		fp_req = L4_FpageLog2( paddr + pt, PAGE_BITS);
		idl4_set_rcv_window( &ipc_env, fp_recv);
		IResourcemon_request_device( L4_Pager(), fp_req.raw, L4_FullyAccessible, &fp, &ipc_env );
		vcpu.vaddr_stats_update(fault_addr + pt, false);
	    }
	    nilmapping = true;
	    goto done;
	}
	/* Request zero page here */
	map_addr = fault_addr & ~(dev_req_page_size -1);	
	paddr = 0;
    }
    else
	map_addr = paddr + link_addr;
    
    fp_recv = L4_FpageLog2( map_addr, map_page_bits );
    fp_req = L4_FpageLog2( paddr, PAGE_BITS );
    idl4_set_rcv_window( &ipc_env, fp_recv );
    IResourcemon_request_pages( L4_Pager(), fp_req.raw, 7, &fp, &ipc_env );

    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &ipc_env );
	PANIC( "IPC request failure to the pager -- ip: %x, fault %x, request %x\n"
		, fault_ip, fault_addr, L4_Address(fp_req));
    }

    if( L4_IsNilFpage(idl4_fpage_get_page(fp)) ) {
	con << "The resource monitor denied a page request at "
	    << (void *)L4_Address(fp_req)
	    << ", size " << (1 << map_page_bits) << '\n';
	panic();
    }
    
    nilmapping = ((fault_addr & PAGE_MASK) == (map_addr & PAGE_MASK));
    goto done;
    
 not_present:
    if( debug_page_not_present )
	con << "page not present, fault addr " << (void *)fault_addr
	    << ", ip " << (void *)fault_ip << '\n';
    if( tid != vcpu.main_gtid )
	PANIC( "Fatal page fault (page not present) in L4 thread %t, address %x, ip %x", 
		tid, fault_addr, fault_ip);
    cpu.cr2 = fault_addr;
    if( deliver_ia32_vector(cpu, 14, (fault_rwx & 2) | 0, ti )) {
	map_addr = fault_addr;
	nilmapping = true; 
	goto done;
    }
    goto unhandled;

 permissions_fault:
    if (debug)
	con << "Delivering page fault for addr " << (void *)fault_addr
	    << ", permissions " << fault_rwx 
	    << ", ip " << (void *)fault_ip << '\n';    
    if( tid != vcpu.main_gtid )
	PANIC( "Fatal page fault (permissions) in L4 thread %t, address %x, ip %x",
		tid, fault_addr, fault_ip);
    cpu.cr2 = fault_addr;
    if( deliver_ia32_vector(cpu, 14, (fault_rwx & 2) | 1, ti)) {
	map_addr = fault_addr;
	nilmapping = true; 
	goto done;
    }
    /* fall through */

 unhandled:
    con << "Unhandled page permissions fault, fault addr " << (void *)fault_addr
	<< ", ip " << (void *)fault_ip << ", fault rwx " << fault_rwx << '\n';
    panic();
   
 done:    
    
    if (nilmapping)
	map_item = L4_MapItem( L4_Nilpage, 0 );
    else
	map_item = L4_MapItem( 
	    L4_FpageAddRights(L4_FpageLog2(map_addr, map_page_bits), map_rwx),
	    ti->mr_save.get_pfault_addr());

    ti->mr_save.load_pfault_reply(map_item);
    return ti;
}


extern "C" void async_irq_handle_exregs( void );
__asm__ ("\n\
	.text								\n\
	.global async_irq_handle_exregs					\n\
async_irq_handle_exregs:						\n\
	pushl	%eax							\n\
	pushl	%ebx							\n\
	movl	(4+8)(%esp), %eax	/* old stack */			\n\
	subl	$16, %eax		/* allocate iret frame */	\n\
	movl	(16+8)(%esp), %ebx	/* old flags */			\n\
	movl	%ebx, 12(%eax)						\n\
	movl	(12+8)(%esp), %ebx	/* old cs */			\n\
	movl	%ebx, 8(%eax)						\n\
	movl	(8+8)(%esp), %ebx	/* old eip */			\n\
	movl	%ebx, 4(%eax)						\n\
	movl	(0+8)(%esp), %ebx	/* new eip */			\n\
	movl	%ebx, 0(%eax)						\n\
	xchg	%eax, %esp		/* swap to old stack */		\n\
	movl	(%eax), %ebx		/* restore ebx */		\n\
	movl	4(%eax), %eax		/* restore eax */		\n\
	ret				/* activate handler */		\n\
	");


bool backend_async_irq_deliver( intlogic_t &intlogic )
{
    vcpu_t &vcpu = get_vcpu();
    cpu_t &cpu = vcpu.cpu;

    ASSERT( L4_MyLocalId() != vcpu.main_ltid );
    
    word_t vector, irq;

#if defined(CONFIG_L4KA_VMEXT)
    if( EXPECT_FALSE(!async_safe(vcpu.main_info.mr_save.get(OFS_MR_SAVE_EIP))))
    {
	/* 
	 * We are already executing somewhere in the wedge. We don't deliver
	 * interrupts directly but reply with an idempotent preemption message
	 * (unless we're idle)
	 */
	return vcpu.redirect_idle();
    }
#endif
    if( !cpu.interrupts_enabled() )
	return false;
    if( !intlogic.pending_vector(vector, irq) )
	return false;

   
    if( debug_irq_deliver || intlogic.is_irq_traced(irq)  )
 	con << "Interrupt deliver, vector " << vector << '\n';
 
    ASSERT( vector < cpu.idtr.get_total_gates() );
    gate_t *idt = cpu.idtr.get_gate_table();
    gate_t &gate = idt[ vector ];

    ASSERT( gate.is_trap() || gate.is_interrupt() );
    ASSERT( gate.is_present() );
    ASSERT( gate.is_32bit() );

    if( debug_irq_deliver || intlogic.is_irq_traced(irq)  )
 	con << "Delivering async vector " << vector
 	    << ", handler ip " << (void *)gate.get_offset() << '\n';
 
    L4_Word_t old_esp, old_eip, old_eflags;
    
#if defined(CONFIG_L4KA_VMEXT)
    old_esp = vcpu.main_info.mr_save.get(OFS_MR_SAVE_ESP);
    old_eip = vcpu.main_info.mr_save.get(OFS_MR_SAVE_EIP); 
    old_eflags = vcpu.main_info.mr_save.get(OFS_MR_SAVE_EFLAGS); 

    cpu.flags.x.raw = (old_eflags & flags_user_mask) | (cpu.flags.x.raw & ~flags_user_mask);

    L4_Word_t *esp = (L4_Word_t *) old_esp;
    *(--esp) = cpu.flags.x.raw;
    *(--esp) = cpu.cs;
    *(--esp) = old_eip;
    
    vcpu.main_info.mr_save.set(OFS_MR_SAVE_EIP, gate.get_offset()); 
    vcpu.main_info.mr_save.set(OFS_MR_SAVE_ESP, (L4_Word_t) esp); 
#else
    static const L4_Word_t temp_stack_words = 64;
    static L4_Word_t temp_stacks[ temp_stack_words * CONFIG_NR_CPUS ];
    L4_Word_t dummy;
    
    L4_Word_t *esp = (L4_Word_t *)
	&temp_stacks[ (vcpu.get_id()+1) * temp_stack_words ];

    esp = &esp[-5]; // Allocate room on the temp stack.

    L4_ThreadId_t dummy_tid, result_tid;
    L4_ThreadState_t l4_tstate;
    
    result_tid = L4_ExchangeRegisters( vcpu.main_ltid, (3 << 3) | 2,
	    (L4_Word_t)esp, (L4_Word_t)async_irq_handle_exregs,
	    0, 0, L4_nilthread,
	    &l4_tstate.raw, &old_esp, &old_eip, &old_eflags,
	    &dummy, &dummy_tid );
    
    ASSERT( !L4_IsNilThread(result_tid) );

    if( EXPECT_FALSE(!async_safe(old_eip)) ) 
	// We are already executing somewhere in the wedge.
    {
	// Cancel the interrupt delivery.
	INC_BURN_COUNTER(async_delivery_canceled);
	intlogic.reraise_vector( vector, irq ); // Reraise the IRQ.
	
	// Resume execution at the original esp + eip.
	result_tid = L4_ExchangeRegisters( vcpu.main_ltid, (3 << 3) | 2,
		old_esp, old_eip, 0, 0, 
		L4_nilthread, &l4_tstate.raw, 
		&old_esp, &old_eip, &old_eflags, 
		&dummy, &dummy_tid );
	ASSERT( !L4_IsNilThread(result_tid) );
	ASSERT( old_eip == (L4_Word_t)async_irq_handle_exregs );
	return false;
    }
    
    cpu.flags.x.raw = (old_eflags & flags_user_mask) | (cpu.flags.x.raw & ~flags_user_mask);
    
    // Store values on the stack.
    esp[4] = cpu.flags.x.raw;
    esp[3] = cpu.cs;
    esp[2] = old_eip;
    esp[1] = old_esp;
    esp[0] = gate.get_offset();
  
#endif 
    // Side effects are now permitted to the CPU object.
    cpu.flags.prepare_for_gate( gate );
    cpu.cs = gate.get_segment_selector();
    return true;
}

word_t backend_phys_to_dma_hook( word_t phys )
{
    word_t dma;

    if( EXPECT_FALSE(phys > resourcemon_shared.phys_end) ) {
	con << "Fatal DMA error: excessive address " << (void *)phys
	    << " > " << (void *)resourcemon_shared.phys_end << '\n';
	panic();
    }

    if( phys < resourcemon_shared.phys_size )
	// Machine memory that resides in the VM.
	dma = phys + resourcemon_shared.phys_offset;
    else
    {   
        // Memory is arranged such that the VM's memory is swapped with
        // lower memory.  The memory above the VM is 1:1.
        if ((phys-resourcemon_shared.phys_size) < resourcemon_shared.phys_offset)
            // Machine memory which resides before the VM.
            dma = phys - resourcemon_shared.phys_size;
        else
            // Machine memory which resides after the VM.
            dma = phys;
    }

    if (debug_dma)
	con << "phys to dma, " << (void *)phys << " to " << (void *)dma << '\n';
    return dma;
}

word_t backend_dma_to_phys_hook( word_t dma )
{
    unsigned long vm_offset = resourcemon_shared.phys_offset;
    unsigned long vm_size = resourcemon_shared.phys_size;
    unsigned long paddr;

    if ((dma >= 0x9f000) && (dma < 0x100000))
        // An address within a shadow BIOS region?
        paddr = dma;
    else if (dma < vm_offset)
        // Machine memory which resides before the VM.
        paddr = dma + vm_size;
    else if (dma < (vm_offset + vm_size))
        // Machine memory in the VM.
        paddr = dma - vm_offset;
    else
        // Machine memory which resides above the VM.
        paddr = dma;

    if( EXPECT_FALSE(paddr > resourcemon_shared.phys_end) )
	con << "DMA range error\n";

    if (debug_dma)
	con<< "dma to phys, " << (void *)dma << " to " << (void *)paddr << '\n';
    return paddr;
}


bool backend_request_device_mem( word_t base, word_t size, word_t rwx, bool boot)
{
    CORBA_Environment ipc_env = idl4_default_environment;
    idl4_fpage_t idl4fp;
    L4_Fpage_t fp = L4_Fpage ( base, size);
    idl4_set_rcv_window( &ipc_env, fp );

    IResourcemon_request_device( 
	    resourcemon_shared.cpu[get_vcpu().pcpu_id].resourcemon_tid, 
	    fp.raw, 
	    rwx, 
	    &idl4fp, 
	    &ipc_env );
    
    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	word_t err = CORBA_exception_id(&ipc_env);
	CORBA_exception_free( &ipc_env );
	
	if (debug_device)
	    con << "backend_request_device_mem: error " << err 
		<< " base " << (void*) base << "\n";

	return false;
    }
    return true;
}    
    

bool backend_unmap_device_mem( word_t base, word_t size, word_t &rwx, bool boot)
{
    
    CORBA_Environment ipc_env = idl4_default_environment;
    L4_Fpage_t fp = L4_Fpage ( base, size);

    L4_Word_t old_rwx;
    
    IResourcemon_unmap_device(
	resourcemon_shared.cpu[get_vcpu().pcpu_id].resourcemon_tid, 
	fp.raw,
	rwx, 
	&old_rwx,
	&ipc_env );

    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	word_t err = CORBA_exception_id(&ipc_env);
	CORBA_exception_free( &ipc_env );
	con << "backend_unmap_device_mem: error " << err 
	    << " base " << (void*) base << "\n";
	
	rwx = 0;
	return false;
    }
    
    rwx = old_rwx;
    return true;
}

void backend_unmap_acpi_mem() {
    UNIMPLEMENTED();
}

word_t backend_map_acpi_mem(word_t base) {
    UNIMPLEMENTED();
    return base;
}
