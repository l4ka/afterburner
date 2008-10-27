/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4ka/backend.cc
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
#include <burn_counters.h>
#include <l4/kip.h>
#include <console.h>
#include <debug.h>
#include <device/acpi.h>
#include <device/apic.h>
#include <device/dp83820.h>
#include <device/i82371ab.h>
#include INC_ARCH(ia32.h)
#include INC_ARCH(page.h)
#include INC_ARCH(cpu.h)
#include INC_ARCH(intlogic.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(resourcemon.h)
#include INC_WEDGE(dspace.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(l4privileged.h)

DECLARE_BURN_COUNTER(async_delivery_canceled);

#if defined(CONFIG_QEMU_DM) && defined(CONFIG_L4KA_HVM)
#include INC_WEDGE(qemu_mmio.h)
#endif

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



thread_info_t * backend_handle_pagefault( L4_MsgTag_t tag, L4_ThreadId_t tid )
{
    vcpu_t &vcpu = get_vcpu();
    thread_info_t *ti = NULL;
    word_t l4thread;
    word_t fault_addr = 0, fault_ip = 0, fault_rwx = 0;
    const word_t link_addr = vcpu.get_kernel_vaddr();
    
    L4_MapItem_t map_item;
    word_t paddr = 0;
    bool nilmapping = false;

    idl4_fpage_t fp;
    CORBA_Environment ipc_env = idl4_default_environment;

    if( L4_UntypedWords(tag) != 2 ) {
	printf( "Bogus page fault message from TID %t\n", tid);
	return NULL;
    }
    
    if (tid == vcpu.main_gtid || tid == vcpu.main_ltid)
	ti = &vcpu.main_info;
    else if (tid == vcpu.irq_gtid || tid == vcpu.irq_ltid)
	ti = &vcpu.irq_info;
#if defined(CONFIG_VSMP)
    else if (vcpu.is_booting_other_vcpu()
	     && tid == get_vcpu(vcpu.get_booted_cpu_id()).monitor_gtid)
	ti = &get_vcpu(vcpu.get_booted_cpu_id()).monitor_info;
#endif
    else if (vcpu.is_vcpu_thread(tid, l4thread))
	ti = &vcpu.l4thread_info[l4thread];
    else
    {
	mrs_t tmp;
	tmp.store();
	
	printf( "invalid pfault message, VCPU %d, addr %x, ip %x rwx %x TID %t\n",
		vcpu.cpu_id, tmp.get_pfault_addr(), tmp.get_pfault_ip(),
		tmp.get_pfault_rwx(), tid);
	return NULL;
    }
    
    ti->mrs.store();

    fault_addr = ti->mrs.get_pfault_addr();
    fault_ip = ti->mrs.get_pfault_ip();
    fault_rwx = ti->mrs.get_pfault_rwx();

    dprintf(debug_pfault, "pfault VCPU %d, addr %x, ip %x rwx %x TID %t\n",
	    vcpu.cpu_id, fault_addr, fault_ip, fault_rwx, tid);
    
    ti->mrs.dump(debug_pfault+1);
    
    map_info_t map_info = { fault_addr, DEFAULT_PAGE_BITS, 7 } ; L4_Fpage_t fp_recv, fp_req;
    word_t dev_req_page_size = PAGE_SIZE;
    // Get the whole superpage for devices
    //dev_req_page_size = (1UL << page_bits);
    
#if defined(CONFIG_DEVICE_DP83820) 
    {
	dp83820_t *dp83820 = dp83820_t::get_pfault_device(fault_addr);
	if( dp83820 ) 
	{
	    dp83820->backend.handle_pfault(dp83820, fault_addr, fault_rwx);
	    nilmapping = true;
	    goto done;
	}
    }
#endif

#if defined(CONFIG_DEVICE_I82371AB) && !defined(CONFIG_L4KA_HVM)
    if( i82371ab_t::get_pfault_device(fault_addr) ) {
	L4_Fpage_t req_fp = L4_Fpage( fault_addr & PAGE_MASK, 4096 );
	idl4_set_rcv_window( &ipc_env, L4_CompleteAddressSpace );
	IResourcemon_request_pages(  resourcemon_shared.resourcemon_tid, req_fp.raw, 0, &fp, &ipc_env);
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

	printf( "DSPACE pfault VCPU %d, addr %x, ip %x rwx %x TID %t\n",
		vcpu.cpu_id, fault_addr, fault_ip, fault_rwx, tid);

	nilmapping = (MASK_BITS(fault_addr, map_info.page_bits) == MASK_BITS(map_info.addr, map_info.page_bits));
	goto done;
    }
    
    if (vcpu.resolve_paddr(ti, map_info, paddr, nilmapping))
	goto done;
    
#if defined(CONFIG_DEVICE_APIC) 
    if (acpi.handle_pfault(ti, map_info, paddr, nilmapping))
	goto done;
#endif

#if defined(CONFIG_QEMU_DM) && defined(CONFIG_L4KA_HVM)
    if(qemu_is_mmio_area(fault_addr))
    {
	dprintf(debug_qemu,"mmio access, vaddr %x map_info.addr %x, paddr %x, ip %x\n",
		fault_addr, map_info.addr, paddr, fault_ip);
	handle_mmio(fault_addr);

	nilmapping = true;
	goto done;
	  
    }
#endif

    if (contains_device_mem(paddr, paddr + (dev_req_page_size - 1)))
    {
	
	dprintf(debug_device, "device access, vaddr %x map_info.addr %x, paddr %x, size %08d, ip %x\n",
		fault_addr, map_info.addr, paddr, dev_req_page_size, fault_ip);
	
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
#if !defined(CONFIG_L4KA_HVM)
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
    {
	map_info.addr = paddr + link_addr;
    }
    
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
	printf( "Resource monitor denied a page request at %x, size %08d\n",
		L4_Address(fp_req), 1 << map_info.page_bits);
	panic();
    }
   
#if !defined(CONFIG_L4KA_HVM)
    nilmapping = ((fault_addr & PAGE_MASK) == (map_info.addr & PAGE_MASK));
#endif
    
done:    

    if (nilmapping)
    {
	dprintf(debug_pfault, "pfault reply TID %t nilmapping\n", tid);
	map_item = L4_MapItem( L4_Nilpage, 0 );
    }
    else
    {
	dprintf(debug_pfault, "pfault reply TID %t paddr %x maddr %x size %08d rwx %x\n", 
		tid, paddr, map_info.addr, map_info.page_bits, map_info.rwx);
	
	map_item = L4_MapItem( 
	    L4_FpageAddRights(L4_FpageLog2(map_info.addr, map_info.page_bits), map_info.rwx),
	    fault_addr);
    }
    
    ti->mrs.load_pfault_reply(map_item);
    return ti;
}



word_t backend_phys_to_dma_hook( word_t phys )
{
    unsigned long vm_offset = resourcemon_shared.phys_offset;
    unsigned long vm_size = resourcemon_shared.phys_size;
    unsigned long vm_end = resourcemon_shared.phys_end;

    word_t dma;

    if( EXPECT_FALSE(phys > vm_end) ) {
	printf( "Fatal DMA error: excessive address %x end %x\n",
		phys, vm_end);
	panic();
    }

    if( phys < vm_size )
	// Machine memory that resides in the VM.
	dma = phys + vm_offset;
    else
    {   
        // Memory is arranged such that the VM's memory is swapped with
        // lower memory.  The memory above the VM is 1:1.
        if ((phys-vm_size) < vm_offset)
            // Machine memory which resides before the VM.
            dma = phys - vm_size;
        else
            // Machine memory which resides after the VM.
            dma = phys;
    }
    dprintf(debug_dma, "phys %x to dma %x vm offset %x size %d\n", 
	    phys, dma, vm_offset, vm_size);

    return dma;
}

word_t backend_dma_to_phys_hook( word_t dma )
{
    unsigned long vm_offset = resourcemon_shared.phys_offset;
    unsigned long vm_size = resourcemon_shared.phys_size;
    unsigned long vm_end = resourcemon_shared.phys_end;
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

    if( EXPECT_FALSE(paddr > vm_end) )
	printf( "DMA range error\n");

    dprintf(debug_dma, "dma %x to phys %x vm offset %x rmon size %d\n", 
	    dma, paddr, vm_offset, vm_size);
    
    return paddr;
}


bool backend_request_device_mem( word_t base, word_t size, word_t rwx, bool boot, word_t receive_addr)
{
    CORBA_Environment ipc_env = idl4_default_environment;
    idl4_fpage_t idl4fp;
    L4_Fpage_t fp = L4_Fpage ( base, size);
    if (receive_addr == 0)
	idl4_set_rcv_window( &ipc_env, fp );
    else
	idl4_set_rcv_window( &ipc_env, L4_Fpage(receive_addr, size));
    
    IResourcemon_request_device( resourcemon_shared.resourcemon_tid, 
				 fp.raw, rwx, &idl4fp, &ipc_env );
    
    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	UNUSED word_t err = CORBA_exception_id(&ipc_env);
	CORBA_exception_free( &ipc_env );
	
	dprintf(debug_device, "backend_request_device_mem: base %x error %d\n", base, 
		CORBA_exception_id(&ipc_env));

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
    
    IResourcemon_request_device( 
	    resourcemon_shared.resourcemon_tid, 
	    fp.raw, rwx, &idl4fp, &ipc_env 
	);
    
    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &ipc_env );
	
	dprintf(debug_device, "backend_request_device_mem: base %x error %d\n", 
		base, CORBA_exception_id(&ipc_env));	

	return false;
    }
    return true;
}    


bool backend_unmap_device_mem( word_t base, word_t size, word_t &rwx, bool boot)
{
    
    CORBA_Environment ipc_env = idl4_default_environment;
    L4_Fpage_t fp = L4_Fpage ( base, size);

    L4_Word_t old_rwx = 0;
   
    IResourcemon_unmap_device(resourcemon_shared.resourcemon_tid, fp.raw, rwx, &old_rwx, &ipc_env );

    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &ipc_env );
	printf( "backend_unmap_device_mem: base %x error %d\n", base, 
		CORBA_exception_id(&ipc_env));
	rwx = 0;
	return false;
    }
    
    rwx = old_rwx;
    return true;
}

time_t backend_get_unix_seconds()
{
    return (time_t) (L4_SystemClock().raw / (1000 * 1000));
}

L4_MsgTag_t backend_notify_thread( L4_ThreadId_t tid, L4_Time_t timeout)
{
#if defined(CONFIG_L4KA_VMEXT)
    dprintf(debug_preemption, "backend notify %t\n", tid);
    if( tid == get_vcpu().get_hwirq_tid() ) 
	return L4_Ipc( tid, L4_anythread, L4_Timeouts(timeout, L4_Never), &tid);
#endif
    return L4_Send( tid, timeout );
}

void backend_flush_user( word_t pdir_paddr )
{
    vcpu_t &vcpu = get_vcpu();

#if defined(CONFIG_L4KA_HVM)
    UNIMPLEMENTED();
#endif
    
#if defined(CONFIG_L4KA_VMEXT)
    L4_ThreadId_t tid;
    task_info_t *ti;
    thread_info_t *thi;

    if ((ti = get_task_manager().find_by_page_dir(pdir_paddr, false)) &&
	(thi = ti->get_vcpu_thread(vcpu.cpu_id, false)))
	tid = thi->get_tid();
    else
	tid = L4_nilthread;
	
    dprintf(debug_task, "flush user pdir %wx tid %t\n", 
	    pdir_paddr, tid);
#endif
    
#if 0
    L4_Flush( L4_CompleteAddressSpace + L4_FullyAccessible );
#else
    // Pistachio is currently broken, and unmaps the kip and utcb.
    if( vcpu.get_kernel_vaddr() == GB(3UL) )
    {
    	L4_Flush( L4_FpageLog2(0, 31) + L4_FullyAccessible );
    	L4_Flush( L4_FpageLog2(GB(2), 30) + L4_FullyAccessible );
    }
    else if( vcpu.get_kernel_vaddr() == GB(2UL) )
    	L4_Flush( L4_FpageLog2(0, 31) + L4_FullyAccessible );
    else if( vcpu.get_kernel_vaddr() == GB(1UL) )
    	L4_Flush( L4_FpageLog2(0, 30) + L4_FullyAccessible );
    else if( vcpu.get_kernel_vaddr() == MB(1536UL) ) {
    	L4_Flush( L4_FpageLog2(0, 30) + L4_FullyAccessible );
    	L4_Flush( L4_FpageLog2(GB(1), 29) + L4_FullyAccessible );
    }
    else if( vcpu.get_kernel_vaddr() == MB(256UL) )
	L4_Flush( L4_Fpage(0, MB(256)) + L4_FullyAccessible );
    else if( vcpu.get_kernel_vaddr() == MB(8UL) )
    	L4_Flush( L4_FpageLog2(0, 23) + L4_FullyAccessible );
    else if( vcpu.get_kernel_vaddr() == 0 ) {
	printf( "Skipping address space flush, and assume that user-apps\n"
		"aren't ever mapped into the kernel's address space.\n");
    }
    else
	PANIC( "Unsupported kernel link address: %x", vcpu.get_kernel_vaddr());
#endif
}

INLINE L4_ThreadId_t interrupt_to_tid( u32_t interrupt )
{
    L4_ThreadId_t tid;
    tid.global.X.thread_no = interrupt;
    tid.global.X.version = 1;
    return tid;
}

bool backend_enable_device_interrupt( u32_t interrupt, vcpu_t &vcpu )
{
    intlogic_t &intlogic = get_intlogic();
    ASSERT( !vcpu.cpu.interrupts_enabled() );
    
    if (intlogic.is_hwirq_squashed(interrupt) || 
	intlogic.is_virtual_hwirq(interrupt))
	return true;
    
    dprintf(irq_dbg_level(interrupt), "enable device irq: %d\n", interrupt);

    L4_Word_t prio = resourcemon_shared.prio + CONFIG_PRIO_DELTA_HWIRQ;
    L4_Word_t cpu = vcpu.cpu_id;
    L4_ThreadId_t irq_tid = L4_nilthread;
    irq_tid.global.X.thread_no = interrupt;
    irq_tid.global.X.version = 1;
    L4_Error_t errcode = AssociateInterrupt( irq_tid, vcpu.irq_gtid, prio, cpu);
    
    if( errcode != L4_ErrOk )
    {
#if !defined(CONFIG_L4KA_VMEXT)
	if (errcode == L4_ErrNoPrivilege && L4_Set_Priority(irq_tid, prio))
	    return true;
	else
	    errcode = L4_ErrorCode();
#endif
	printf( "error associating irq %d L4 error: %s\n",
		interrupt, L4_ErrString(errcode));
	return false;
    }
    return true;
}


bool backend_disable_device_interrupt( u32_t interrupt, vcpu_t &vcpu )
{
    intlogic_t &intlogic = get_intlogic();
    ASSERT( !vcpu.cpu.interrupts_enabled() );
    
    if (intlogic.is_hwirq_squashed(interrupt) || 
	intlogic.is_virtual_hwirq(interrupt))
	return true;
    
    dprintf(irq_dbg_level(interrupt), "disable device irq: %d\n", interrupt);
    L4_ThreadId_t irq_tid = L4_nilthread;
    irq_tid.global.X.thread_no = interrupt;
    irq_tid.global.X.version = 1;
    L4_Error_t errcode = DeassociateInterrupt( irq_tid );
    
    if( errcode != L4_ErrOk )
    {
	printf( "error deassociating irq %d L4 error: %s\n",
		interrupt, L4_ErrString(errcode));
	return false;
    }
    return true;
}


bool backend_unmask_device_interrupt( u32_t interrupt )
{
   
    //ASSERT( !get_vcpu().cpu.interrupts_enabled() );
    L4_ThreadId_t ack_tid;
    L4_MsgTag_t tag = L4_Niltag;
    intlogic_t &intlogic = get_intlogic();
    vcpu_t &vcpu = get_vcpu();
    
    if (intlogic.is_virtual_hwirq(interrupt))
    {	
	ack_tid = intlogic.get_virtual_hwirq_sender(interrupt);	
	dprintf(irq_dbg_level(interrupt), "unmask virtual IRQ %d sender %t\n", interrupt, ack_tid);
	
	if (ack_tid == L4_nilthread)
	    return true;
	
    }
    else 
    {
	if (intlogic.is_hwirq_squashed(interrupt))
	    return true;

	dprintf(irq_dbg_level(interrupt), "unmask IRQ %d\n", interrupt);
	ack_tid = vcpu.get_hwirq_tid(interrupt);
    }
    
    /* Propagate ACK from IRQ tid */
    msg_hwirq_ack_build( interrupt, vcpu.irq_gtid );
    tag = backend_notify_thread(ack_tid, L4_Never);
    
    if (L4_IpcFailed(tag))
    {
	printf( "unmask IRQ %d via propagation failed L4 error: %d\n", 
		interrupt, L4_ErrorCode());
	DEBUGGER_ENTER("BUG");
    }
	    
    return L4_IpcFailed(tag);
}



u32_t backend_get_nr_device_interrupts()
{
    L4_KernelInterfacePage_t *kip = (L4_KernelInterfacePage_t *) L4_GetKernelInterface();
    return  L4_ThreadIdSystemBase(kip);
}

INLINE u32_t word_text( char a, char b, char c, char d )
{
    return (a)<<0 | (b)<<8 | (c)<<16 | (d)<<24;
}


void backend_cpuid_override( 
    u32_t func, u32_t max_basic, u32_t max_extended,
    frame_t *regs )
{
    
    switch( func )
    {
    case 1:
	regs->x.fields.ebx &= 0xFF00FFFF;	 // Disable HT
	
	
	regs->x.fields.ecx &= ~X86_FEATURE_MWAIT;
	regs->x.fields.ecx &= ~X86_FEATURE_VMXE;
	regs->x.fields.ecx &= ~X86_FEATURE_EST;
	regs->x.fields.ecx &= ~X86_FEATURE_TM2;
	regs->x.fields.ecx &= ~X86_FEATURE_CID;
	regs->x.fields.ecx &= ~X86_FEATURE_PDCM;
	regs->x.fields.ecx &= ~X86_FEATURE_DSCPL;
	regs->x.fields.ecx &= ~X86_FEATURE_SMXE;
	regs->x.fields.ecx &= ~X86_FEATURE_DCA;
		    
		    
	regs->x.fields.edx &= ~X86_FEATURE_PAT;
	regs->x.fields.edx &= ~X86_FEATURE_PSE;
	regs->x.fields.edx &= ~X86_FEATURE_PSE36;
	regs->x.fields.edx &= ~X86_FEATURE_MTRR; 
	regs->x.fields.edx &= ~X86_FEATURE_PAE;
	regs->x.fields.edx &= ~X86_FEATURE_DS;
	regs->x.fields.edx &= ~X86_FEATURE_HT;
	regs->x.fields.edx &= ~X86_FEATURE_ACC;
	regs->x.fields.edx &= ~X86_FEATURE_ACPI;
	
#if !defined(CONFIG_DEVICE_APIC)
	regs->x.fields.edx &= ~X86_FEATURE_APIC;
#endif
	break;
    case 4: 
	regs->x.fields.eax &= 0x00003FFF;	// Disable MultiCore
	break;
    case 0x80000001:
	regs->x.fields.ecx &= ~X86_FEATURE_LAHF_LM;

	regs->x.fields.edx &= ~X86_FEATURE_NX;
	regs->x.fields.edx &= ~X86_FEATURE_LM;
	regs->x.fields.edx &= ~X86_FEATURE_SYSCALL;
	
	break;
    case 0x80000002:
	regs->x.fields.eax = word_text('L','4','K','a');
	regs->x.fields.ebx = word_text(':',':','P','i');
	regs->x.fields.ecx = word_text('s','t','a','c');
	regs->x.fields.edx = word_text('h','i','o',' ');
	break;
    case 0x80000003:
	regs->x.fields.eax = word_text('(','h','t','t');
	regs->x.fields.ebx = word_text('p',':','/','/');
	regs->x.fields.ecx = word_text('l','4','k','a');
	regs->x.fields.edx = word_text('.','o','r','g');
	break;
    case 0x80000004:
	regs->x.fields.eax = word_text(')',0,0,0);
	break;
    default: 
	break;
    }
}

void backend_reboot( void )
{
    DEBUGGER_ENTER("VM reboot");
    /* Return to the caller and take a chance at completing a hardware
     * reboot.
     */
}

