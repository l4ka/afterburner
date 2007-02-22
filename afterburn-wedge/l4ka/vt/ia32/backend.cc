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
#include INC_WEDGE(vt/monitor.h)
#include <device/acpi.h>
#include <device/apic.h>
#include <device/dp83820.h>

#include INC_WEDGE(vt/vm.h)

#include INC_WEDGE(message.h)

#include <burn_counters.h>

static const bool debug=0;
static const bool debug_page_not_present=0;
static const bool debug_irq_forward=0;
static const bool debug_irq_deliver=0;
static const bool debug_pfault=1;
static const bool debug_superpages=1;
static const bool debug_user_access=1;
static const bool debug_dma=0;
static const bool debug_apic=0;
static const bool debug_device=1;

DECLARE_BURN_COUNTER(async_delivery_canceled);

dspace_handlers_t dspace_handlers;
thread_info_t ti;

INLINE bool async_safe( word_t ip )
{
    return ip < CONFIG_WEDGE_VIRT;
}

#if 0
void backend_enable_paging( word_t *ret_address )
{
    vmx_vcpu_t &vcpu = get_vcpu();
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
	cpu_t & cpu, L4_Word_t vector, u32_t error_code )
{
    UNIMPLEMENTED();

    return true;
}
#endif

thread_info_t * backend_handle_pagefault( L4_MsgTag_t tag, L4_ThreadId_t tid )
{
    CORBA_Environment ipc_env = idl4_default_environment;
    idl4_fpage_t fp;
    
    word_t fault_addr, ip, map_addr;
    L4_MapItem_t map_item;
    word_t rwx, page_bits;

    if( L4_UntypedWords(tag) != 2 ) {
	con << "Invalid page fault message from TID " << tid << '\n';
	return NULL;
    }

    rwx = L4_Label(tag) & 0x7;
    L4_StoreMR( 1, (L4_Word_t *)&fault_addr );
    L4_StoreMR( 2, (L4_Word_t *)&ip );

    if( debug_pfault )
	con << "pfault, addr: " << (void *)fault_addr 
	    << ", ip: " << (void *)ip << ", rwx: " << (void *)rwx
     	    << ", TID: " << tid << '\n';

    // resolve thread id
    vm_t *vm = get_vm();
    con << "thread id " << tid << " belongs to VM " << (void*) vm << "\n";
    if( vm == 0 ) {
	UNIMPLEMENTED();
    }

    map_addr = get_map_addr( fault_addr );

#if 0
    vcpu_t &vcpu = get_vcpu();
    cpu_t &cpu = vcpu.cpu;
#endif
    L4_Word_t paddr = fault_addr;
    L4_Word_t fault_rwx = rwx;
    L4_Word_t dev_req_page_size;
    
    page_bits = SUPERPAGE_BITS;
    rwx = 7;

    if( debug_pfault )
	con << "Page fault, " << (void *)fault_addr
	    << ", ip " << (void *)ip << ", rwx " << fault_rwx << '\n';

    L4_Fpage_t map_fp;
    if( dspace_handlers.handle_pfault(fault_addr, &ip, &map_fp) )
    {
	// Note: we ignore changes to ip.
	map_addr = L4_Address( map_fp );
	page_bits = L4_SizeLog2( map_fp );
	rwx = L4_Rights( map_fp );
	goto done;
    }

    L4_Fpage_t fp_recv, fp_req;

#if defined(CONFIG_DEVICE_APIC)
    word_t ioapic_id;
    if (acpi.is_lapic_addr(paddr))
    {
	if (debug_apic)
	    con << "LAPIC  access @ " << (void *)fault_addr << " (" << (void *) paddr 
		<< "), mapping softLAPIC @" << (void *) get_cpu().get_lapic() << " \n";
	    
	fp_recv = L4_FpageLog2( map_addr, PAGE_BITS );
	    
	word_t lapic_paddr = (word_t) get_cpu().get_lapic() - 
	    get_vcpu().get_wedge_vaddr() + get_vcpu().get_wedge_paddr();
	    
	fp_req = L4_FpageLog2(lapic_paddr, PAGE_BITS );
	idl4_set_rcv_window( &ipc_env, fp_recv );
	IResourcemon_request_pages( L4_Pager(), fp_req.raw, 5, &fp, &ipc_env );
	goto done;
    }
    else if (acpi.is_ioapic_addr(paddr, &ioapic_id))
    {
	word_t ioapic_addr = 0;
	intlogic_t &intlogic = get_intlogic();
	for (word_t idx = 0; idx < CONFIG_MAX_IOAPICS; idx++)
	    if ((intlogic.ioapic[idx].get_id()) == ioapic_id)
		ioapic_addr = (word_t) &intlogic.ioapic[idx];
	    
	if (ioapic_addr == 0)
	{
	    con << "BUG: Access to nonexistent IOAPIC (id " << ioapic_id << ")\n"; 
	    L4_KDB_Enter("BUG");
	    panic();
	}
		    
	if (debug_apic)
	    con << "IOAPIC access @ " << (void *)fault_addr 
		<< " (id" << ioapic_id << " / "   << (void *) paddr  
		<< "), mapping softIOAPIC @ " << (void *) ioapic_addr << " \n";

	fp_recv = L4_FpageLog2( map_addr, PAGE_BITS );
	word_t ioapic_paddr = (word_t) ioapic_addr -
	    get_vcpu().get_wedge_vaddr() + get_vcpu().get_wedge_paddr();
	fp_req = L4_FpageLog2(ioapic_paddr, PAGE_BITS );
	idl4_set_rcv_window( &ipc_env, fp_recv );
	IResourcemon_request_pages( L4_Pager(), fp_req.raw, 5, &fp, &ipc_env );
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

#if defined(CONFIG_DEVICE_PASSTHRU)
    if (contains_device_mem(paddr, paddr + (dev_req_page_size - 1)))
	goto device_access;
#endif
    
    map_addr &= ~((1UL << page_bits) - 1);
    paddr &= ~((1UL << page_bits) - 1);
    fp_recv = L4_FpageLog2( map_addr, page_bits );
    //fp_req = L4_FpageLog2( paddr, page_bits );
    fp_req = fp_recv;
    idl4_set_rcv_window( &ipc_env, fp_recv );
    IResourcemon_request_pages( L4_Pager(), fp_req.raw, 7, &fp, &ipc_env );

    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &ipc_env );
	PANIC( "IPC request failure to the pager -- ip %x, fault %x, request %x\n",
	       ip, fault_addr, L4_Address(fp_req));
    }

    if( L4_IsNilFpage(idl4_fpage_get_page(fp)) ) {
	con << "The resource monitor denied a page request at "
	    << (void *)L4_Address(fp_req)
	    << ", size " << (1 << page_bits) << '\n';
	panic();
    }

    goto done;

#if defined(CONFIG_DEVICE_PASSTHRU)
device_access:
    // Temporary hack, see above
    page_bits = PAGE_BITS;

    map_addr &= ~(dev_req_page_size - 1);
    paddr &= ~(dev_req_page_size - 1);
 
    if (debug_device)
	con << "device access, vaddr " << (void *)fault_addr
	    << ", map_addr " << (void *)map_addr
	    << ", paddr " << (void *)paddr 
	    << ", size "  << dev_req_page_size
	    << ", ip " << (void *)ip << '\n';

    for (word_t pt=0; pt < dev_req_page_size ; pt+= PAGE_SIZE)
    {
	fp_recv = L4_FpageLog2( map_addr + pt, PAGE_BITS );
	fp_req = L4_FpageLog2( paddr + pt, PAGE_BITS );
	idl4_set_rcv_window( &ipc_env, fp_recv );
	IResourcemon_request_device( L4_Pager(), fp_req.raw, L4_FullyAccessible, &fp, &ipc_env );
	//vcpu.vaddr_stats_update(map_addr + pt, false);
    }
    
#endif
done:
    map_item = L4_MapItem( 
	      	L4_FpageAddRights(L4_FpageLog2(map_addr, page_bits), rwx),
	       	fault_addr );

    L4_Msg_t msg;
    L4_MsgClear( &msg );
    L4_MsgAppendMapItem( &msg, map_item );
    L4_MsgLoad( &msg );
    ti.mr_save.load_pfault_reply(map_item);
    return &ti;
	

    
}

bool backend_async_irq_deliver( intlogic_t &intlogic )
{
    get_vm()->get_vcpu().deliver_interrupt();
    return true;
}

#if 0
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

    IResourcemon_request_device( L4_Pager(), fp.raw, rwx, &idl4fp, &ipc_env );
    
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
	resourcemon_shared.cpu[get_vcpu().cpu_id].resourcemon_tid, 
	fp.raw,
	rwx, 
	&old_rwx,
	&ipc_env );

    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	word_t err = CORBA_exception_id(&ipc_env);
	CORBA_exception_free( &ipc_env );
	
	if (debug_device)
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
#endif
