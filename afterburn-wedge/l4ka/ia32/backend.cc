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

static const bool debug_irq_forward=0;
static const bool debug_irq_deliver=0;
static const bool debug_dma=0;

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


thread_info_t * backend_handle_pagefault( L4_MsgTag_t tag, L4_ThreadId_t tid )
{
    vcpu_t &vcpu = get_vcpu();

    thread_info_t *ti = NULL;
    word_t fault_addr = 0, fault_ip = 0, fault_rwx = 0;
    const word_t link_addr = vcpu.get_kernel_vaddr();
    
    L4_MapItem_t map_item;
    word_t paddr = 0;
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
    else if (vcpu.is_vcpu_hthread(tid))
	ti = &vcpu.hthread_info;
#if defined(CONFIG_VSMP)
    else if (vcpu.is_booting_other_vcpu()
	    && tid == get_vcpu(vcpu.get_booted_cpu_id()).monitor_gtid)
	ti = &get_vcpu(vcpu.get_booted_cpu_id()).monitor_info;
#endif
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
    
    map_info_t map_info = { vcpu.get_map_addr(fault_addr) , DEFAULT_PAGE_BITS, 7 } ;
    L4_Fpage_t fp_recv, fp_req;
    word_t dev_req_page_size = PAGE_SIZE;
    // Get the whole superpage for devices
    //dev_req_page_size = (1UL << page_bits);

#if defined(CONFIG_DEVICE_DP83820) 
    dp83820_t *dp83820 = dp83820_t::get_pfault_device(ti->mr_save.get_pfault_addr());
    if( dp83820 ) 
    {
	dp83820->backend.handle_pfault( ti->mr_save.get_pfault_addr() );
	nilmapping = true;
	goto done;
    }
#endif
    
    if (vcpu.handle_wedge_pfault(ti, map_info, nilmapping))
	goto done;
	
    L4_Fpage_t map_fp;
    if( dspace_handlers.handle_pfault(fault_addr, &fault_ip, &map_fp) )
    {
	// Note: we ignore changes to ip.
	map_info.addr = L4_Address( map_fp );
	map_info.page_bits = L4_SizeLog2( map_fp );
	map_info.rwx = L4_Rights( map_fp );

	con << "DSpace Page fault, " << (void *)fault_addr
	    << ", ip " << (void *)fault_ip << ", rwx " << fault_rwx << '\n';

	nilmapping = (MASK_BITS(fault_addr, map_info.page_bits) == MASK_BITS(map_info.addr, map_info.page_bits));
	goto done;
    }
    
 

    if (vcpu.resolve_paddr(ti, map_info, paddr, nilmapping))
	goto done;
    
#if defined(CONFIG_DEVICE_APIC) 
    if (acpi.handle_pfault(ti, map_info, paddr, nilmapping))
	goto done;
#endif


    if (contains_device_mem(paddr, paddr + (dev_req_page_size - 1)))
    {
#if defined(CONFIG_L4KA_VT)
	/* do not map real rombios/vgabios */
	if( (fault_addr >= 0xf0000 && fault_addr <= 0xfffff) ||
	    (fault_addr >= 0xc0000 && fault_addr <= 0xc7fff)) {
	    if(debug_device)
		con << "bios access, vaddr " << (void *)fault_addr
		    << ", map_info.addr " << (void *)map_info.addr
		    << ", paddr " << (void *)paddr 
		    << ", size "  << dev_req_page_size
		    << ", ip " << (void *)fault_ip 
		    << '\n';	    
	    map_info.addr = paddr + link_addr;
	    goto cont;
	}
#endif
	if (debug_device)
	    con << "device access, vaddr " << (void *)fault_addr
		<< ", map_info.addr " << (void *)map_info.addr
		<< ", paddr " << (void *)paddr 
		<< ", size "  << dev_req_page_size
		<< ", ip " << (void *)fault_ip 
		<< '\n';
	
	if (is_passthrough_mem(paddr))
	{	
	    map_info.addr = fault_addr & ~(dev_req_page_size -1);	
	    paddr &= ~(dev_req_page_size -1);
	    for (word_t pt=0; pt < dev_req_page_size ; pt+= PAGE_SIZE)
	    {
		fp_recv = L4_FpageLog2( map_info.addr + pt, PAGE_BITS );
		fp_req = L4_FpageLog2( paddr + pt, PAGE_BITS);
		idl4_set_rcv_window( &ipc_env, fp_recv);
		IResourcemon_request_device( L4_Pager(), fp_req.raw, L4_FullyAccessible, &fp, &ipc_env );
		vcpu.vaddr_stats_update(fault_addr + pt, false);
	    }
#if !defined(CONFIG_L4KA_VT)
	    nilmapping = true;
#endif
	    goto done;
	}
	/* Request zero page here */
	map_info.page_bits = PAGE_BITS;
	map_info.addr = fault_addr & ~(dev_req_page_size -1);	
	paddr = 0;
    }
    else
	map_info.addr = paddr + link_addr;
 cont:   
    map_info.addr &= ~((1UL << DEFAULT_PAGE_BITS) - 1);
    fp_recv = L4_FpageLog2( map_info.addr, DEFAULT_PAGE_BITS );
    
    paddr &= ~((1UL << DEFAULT_PAGE_BITS) - 1);
    fp_req = L4_FpageLog2( paddr, DEFAULT_PAGE_BITS );
    
    idl4_set_rcv_window( &ipc_env, fp_recv );
    IResourcemon_request_pages( L4_Pager(), fp_req.raw, 7, &fp, &ipc_env );

    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &ipc_env );
	PANIC( "IPC request failure to the pager -- ip: %x, fault %x, request %x\n", fault_ip, fault_addr, L4_Address(fp_req));
    }

    if( L4_IsNilFpage(idl4_fpage_get_page(fp)) ) {
	con << "The resource monitor denied a page request at "
	    << (void *)L4_Address(fp_req)
	    << ", size " << (1 << map_info.page_bits) << '\n';
	panic();
    }
   
#if !defined(CONFIG_L4KA_VT)
#warning jsXXX: revise nilmapping logic
    nilmapping = ((fault_addr & PAGE_MASK) == (map_info.addr & PAGE_MASK));
#endif
    
 done:    
    if (debug_pfault)
    { 
	
	con << "pfault reply " << vcpu.cpu_id 
	    << ", TID: " << tid ;
	if (nilmapping)
	    con << ", nilmapping\n";
	else
	    con
		<< " paddr: " << (void *) paddr
		<< " maddr: " << (void *) map_info.addr
		<< ", sz: " << map_info.page_bits
		<< ", rwx: " << (void *)  map_info.rwx
		<< "\n";
    }  
    
    if (nilmapping)
	map_item = L4_MapItem( L4_Nilpage, 0 );
    else
	map_item = L4_MapItem( 
	    L4_FpageAddRights(L4_FpageLog2(map_info.addr, map_info.page_bits), map_info.rwx),
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


/*
 * Returns if redirection was necessary
 */
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
	 * We are already executing somewhere in the wedge. Unless we're idle,
	 * we don't deliver interrupts directly.
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
    
    word_t pcpu_id = boot ? 0 : get_vcpu().pcpu_id;
    
    IResourcemon_request_device( 
	    resourcemon_shared.cpu[pcpu_id].resourcemon_tid, 
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

bool backend_request_device_mem_to( word_t base, word_t size, word_t rwx, word_t dest_base, bool boot)
{
    CORBA_Environment ipc_env = idl4_default_environment;
    idl4_fpage_t idl4fp;
    L4_Fpage_t fp = L4_Fpage ( base, size);
    L4_Fpage_t fprec = L4_Fpage ( dest_base, size);
    idl4_set_rcv_window( &ipc_env, fprec );
    
    word_t pcpu_id = boot ? 0 : get_vcpu().pcpu_id;
    
    IResourcemon_request_device( 
	    resourcemon_shared.cpu[pcpu_id].resourcemon_tid, 
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

    L4_Word_t old_rwx = 0;
    word_t pcpu_id = boot ? 0 : get_vcpu().pcpu_id;
    
    IResourcemon_unmap_device(
	resourcemon_shared.cpu[pcpu_id].resourcemon_tid, 
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
