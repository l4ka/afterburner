/*********************************************************************
 *
 * Copyright (C) 2005,2007-2008  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4-common/vm.cc
 * Description:   The L4 state machine for implementing the idle loop,
 *                and the concept of "returning to user".  When returning
 *                to user, enters a dispatch loop, for handling L4 messages.
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

#include <debug.h>
#include <console.h>
#include <l4/schedule.h>
#include <l4/ipc.h>
#include <l4/ipc.h>
#include <l4/schedule.h>
#include <l4/kip.h>
#include <l4/arch.h>
#include <device/portio.h>

#include INC_ARCH(ia32.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(monitor.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(memory.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(module_manager.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(vm.h)
#include INC_ARCH(hvm_vmx.h)

extern unsigned char _binary_rombios_bin_start[];
extern unsigned char _binary_rombios_bin_end[];
extern unsigned char _binary_vgabios_bin_start[];
extern unsigned char _binary_vgabios_bin_end[];
extern word_t afterburn_c_runtime_init;


bool mr_save_t::append_irq(L4_Word_t vector, L4_Word_t irq)
{
    flags_t eflags;
    hvm_vmx_gs_ias_t ias;
    
    // assert that we don't already have a pending exception
    ASSERT (exc_item.item.num_regs == 0);

    eflags.x.raw = gpr_item.regs.eflags;
    ias.raw = nonreg_item.regs.ias;

    if (get_cpu().interrupts_enabled() && ias.raw == 0)
    {
	ASSERT(!get_cpu().cr0.real_mode());
	
	dprintf(irq_dbg_level(irq)+1,
		"INTLOGIC sync irq %d vec %d efl %x (%c) ias %x\n", irq, 
		vector, get_cpu().flags, (get_cpu().interrupts_enabled() ? 'I' : 'i'), 
		ias.raw);
	
	exc_info_t exc;
	exc.raw = 0;
	exc.hvm.type = hvm_vmx_int_t::ext_int;
	exc.hvm.vector = vector;
	exc.hvm.err_code_valid = 0;
	exc.hvm.valid = 1;
	append_exc_item(exc.raw, 0, 0);
	return true;
    }
    
    dprintf(irq_dbg_level(irq)+1,
	    "INTLOGIC sync iwe irq %d vec %d efl %x (%c) ias %x\n", 
	    irq, vector, eflags, (eflags.interrupts_enabled() ? 'I' : 'i'), ias.raw);
    
    // inject IWE 
    hvm_vmx_exectr_cpubased_t cpubased;
    cpubased.raw = execctrl_item.regs.cpu;
    cpubased.iw = 1;
    append_execctrl_item(L4_CTRLXFER_EXEC_CPU, cpubased.raw);

    return false;
}

void mr_save_t::load_startup_reply(L4_Word_t ip, L4_Word_t sp, L4_Word_t cs, L4_Word_t ss, bool real_mode)
{
    tag = startup_reply_tag();
   
    if( real_mode ) 
    {
	printf("untested switch real mode");
	
	gpr_item.regs.eip = ip;
	gpr_item.regs.esp = sp;
	gpr_item.regs.eflags |= (X86_FLAGS_VM | X86_FLAGS_IOPL(3));
	
	// if the disk is larger than 3 MB, assume it is a hard disk
	if( get_vm()->ramdisk_size >= MB(3) ) 
	    gpr_item.regs.edx = 0x80;
	else 
	    gpr_item.regs.edx = 0;
	
	
	//Segment registers (VM8086 mode)
	append_seg_item(L4_CTRLXFER_CSREGS_ID, cs, cs << 4, 0xffff, 0xf3);
	append_seg_item(L4_CTRLXFER_SSREGS_ID, ss, ss << 4, 0xffff, 0xf4);
	append_seg_item(L4_CTRLXFER_DSREGS_ID, 0, 0, 0xffff, 0xf3);
	append_seg_item(L4_CTRLXFER_ESREGS_ID, 0, 0, 0xffff, 0xf3);
	append_seg_item(L4_CTRLXFER_FSREGS_ID, 0, 0, 0xffff, 0xf3);
	append_seg_item(L4_CTRLXFER_GSREGS_ID, 0, 0, 0xffff, 0xf3);
	
	hvm_vmx_segattr_t attr;
	attr.raw		= 0;
	attr.type		= 0x3;
	attr.p			= 1;
	// Mark tss as unused, to consistently generate #GPs
	attr.raw		&= ~(1<<16);
	printf("tr attr %x\n", attr.raw);
	append_seg_item(L4_CTRLXFER_TRREGS_ID, 0, 0, ~0, attr.raw);
	append_seg_item(L4_CTRLXFER_LDTRREGS_ID, 0, 0, ~0, 0x10082);
	
	DEBUGGER_ENTER("UNTESTED");				

    } 
    else 
    { 
	// protected mode
	gpr_item.regs.eip = ip;
	gpr_item.regs.esi = 0x9022;
	
	append_seg_item(L4_CTRLXFER_CSREGS_ID, 1<<3, 0, ~0, 0x0c099);
	append_seg_item(L4_CTRLXFER_SSREGS_ID, 0, 0, 0, 0x10000);
	append_seg_item(L4_CTRLXFER_DSREGS_ID, 2<<3, 0, ~0, 0x0c093);
	append_seg_item(L4_CTRLXFER_ESREGS_ID, 0, 0, 0, 0x10000);
	append_seg_item(L4_CTRLXFER_FSREGS_ID, 0, 0, 0, 0x10000);
	append_seg_item(L4_CTRLXFER_GSREGS_ID, 0, 0, 0, 0x10000);
	append_seg_item(L4_CTRLXFER_TRREGS_ID, 0, 0, ~0, 0x0808b);
	append_seg_item(L4_CTRLXFER_LDTRREGS_ID, 0, 0, 0, 0x18003);
	
	append_cr_item(L4_CTRLXFER_CREGS_CR0, 0x00000031);
	append_cr_item(L4_CTRLXFER_CREGS_CR0_SHADOW, 0x00000031);
	append_cr_item(L4_CTRLXFER_CREGS_CR4_SHADOW, 0x000023d0);
    }
    dump(debug_startup+1);
    append_gpr_item();
}

bool vm_t::init(word_t ip, word_t sp)
{
    
    // find first elf file among the modules, assume that it is the kernel
    // find first non elf file among the modules assume that it is a ramdisk
    guest_kernel_module = NULL;
    ramdisk_start = NULL;
    ramdisk_size = 0;
    entry_ip = ip;
    entry_sp = sp;
    
    module_manager_t *mm = get_module_manager();
    ASSERT( mm );
    
    for( L4_Word_t idx = 0; idx < mm->get_module_count(); ++idx )
    {
	module_t *module = mm->get_module( idx );
	ASSERT( module );
	
	bool valid_elf = elf_is_valid( module->vm_offset );
	if( guest_kernel_module == NULL && valid_elf == true )
	{
	    guest_kernel_module = module;
	}
	else if( ramdisk_size == 0 && valid_elf == false )
	{
	    ramdisk_start = module->vm_offset;
	    ramdisk_size = module->size;
	}
	
	if( guest_kernel_module != NULL && ramdisk_size > 0 )
	{
	    break;
	}
    }
    
    if( ramdisk_size > 0 ) {
	printf( "Found ramdisk at %x size %d\n", ramdisk_start, ramdisk_size);	    
	resourcemon_shared.ramdisk_start = ramdisk_start;
	resourcemon_shared.ramdisk_size = ramdisk_size;
    }
    else 
    {
	printf( "No ramdisk found.\n");
	return false;
    }

    return true;
}

bool vm_t::load_elf( L4_Word_t elf_start )
{
    word_t elf_base_addr, elf_end_addr;
    elf_ehdr_t *ehdr;
    word_t kernel_vaddr_offset;
    
    if( NULL == ( ehdr = elf_is_valid( elf_start ) ) )
    {
	printf( "Not a valid elf binary.\n");
	return false;
    }
    
    // install the binary in the first 64MB
    // convert to paddr
    if( ehdr->entry >= MB(64) )
	entry_ip = (MB(64)-1) & ehdr->entry;
    else
	entry_ip = ehdr->entry;
    
    // infer the guest's vaddr offset from its ELF entry address
    kernel_vaddr_offset = ehdr->entry - entry_ip;
    	    
    if( !ehdr->load_phys( kernel_vaddr_offset ) )
    {
	printf( "Elf loading failed.\n");
	return false;
    }

    ehdr->get_phys_max_min( kernel_vaddr_offset, elf_end_addr, elf_base_addr );

    dprintf(debug_startup, "ELF Binary is residing at guest address [%x-%x] with entry point %x\n",
	    elf_base_addr, elf_end_addr, entry_ip);

    return true;
}



bool vm_t::init_guest( void )
{
    // check requested guest phys mem size
    if( guest_kernel_module != NULL )
    {
	gphys_size = guest_kernel_module->get_module_param_size( "physmem=" );
    }
    
    if( gphys_size == 0 )
    {
	gphys_size = resourcemon_shared.phys_offset;
    } 
   
    // round to 4MB 
    gphys_size &= ~(MB(4)-1);
    
    // Subtract 4MB as scratch memory
    gphys_size -= MB(4);
    wedge_gphys = gphys_size;
    
    dprintf(debug_startup, "%dM of guest phys mem available.\n", (gphys_size >> 20));

#if 0 
    if( gphys_size > (word_t) &afterburn_c_runtime_init )
    {
	printf( "Make sure that the wedge is installed above the maximum guest physical address.\n");
	return false;
    }
#endif
     
    ASSERT( gphys_size > 0 );

    // Copy rombios and vgabios to their dedicated locations
    memmove( (void*)(0xf0000), _binary_rombios_bin_start, _binary_rombios_bin_end - _binary_rombios_bin_start);
    memmove( (void*)(0xc0000), _binary_vgabios_bin_start, _binary_vgabios_bin_end - _binary_vgabios_bin_start);


    // move ramdsk to guest phys space if not already there,
    // or out of guest phys space if we are using it as a real disk
    if( ramdisk_size > 0 ) 
    {
	if( guest_kernel_module == NULL || ramdisk_start + ramdisk_size >= gphys_size ) 
	{
	    L4_Word_t newaddr = gphys_size - ramdisk_size - 1;
	    
	    // align
	    newaddr &= PAGE_MASK;
	    ASSERT( newaddr + ramdisk_size < gphys_size );
	    
	    // move
	    memmove( (void*) newaddr, (void*) ramdisk_start, ramdisk_size );
	    ramdisk_start = newaddr;
	    resourcemon_shared.ramdisk_start = newaddr;
	    resourcemon_shared.ramdisk_size = ramdisk_size;
	}
	if( guest_kernel_module == NULL ) 
	{
	    gphys_size = ramdisk_start;
	    printf( "Reducing guest phys mem to %dM for RAM disk\n", gphys_size >> 20);
	}
    }
    
    // load the guest kernel module
    if( guest_kernel_module == NULL ) 
    {
	if( ramdisk_size < 512 )
	{
	    printf( "No guest kernel module or RAM disk.\n");
	    // set BIOS POST entry point
	    entry_ip = 0xe05b;
	    entry_sp = 0x0000;
	    entry_cs = 0xf000;
	    entry_ss = 0x0000;
	}
	else 
	{
	    // load the boot sector to the fixed location of 0x7c00
	    entry_ip = 0x7c00;
	    entry_sp = 0x0000;
	    entry_cs = 0x0000;
	    entry_ss = 0x0000;
	    // workaround for a bug causing the first byte to be overwritten,
	    // probably in resource monitor
	    *((u8_t*) ramdisk_start) = 0xeb;
	    memmove( 0x0000, (void*) ramdisk_start, ramdisk_size );
	}

    } 
    else 
    {
	if( !load_elf( guest_kernel_module->vm_offset ) )
	{
	    printf( "Loading the guest kernel module failed.\n");
	    return false;
	}

	// save cmdline
	// TODO: really save it in case module gets overwritten somehow
	cmdline = (char*) guest_kernel_module->cmdline_options();
    }
    
    return true;
}


void vcpu_t::load_dispatch_exit_msg(L4_Word_t vector, L4_Word_t irq)
{
    mr_save_t *vcpu_mrs = &main_info.mr_save;

    dispatch_ipc_exit();
    if (!vcpu_mrs->append_irq(vector, irq))
	get_intlogic().reraise_vector(vector, irq);

    vcpu_mrs->load_vfault_reply();

    
    //Inject IRQ or IWE
    vcpu_mrs->set_propagated_reply(monitor_gtid);
    vcpu_mrs->load();
}

bool vcpu_t::startup_vcpu(word_t startup_ip, word_t startup_sp, word_t boot_id, bool bsp)
{
    monitor_gtid = L4_Myself();
    monitor_ltid = L4_MyLocalId();

    // init vcpu
    main_gtid = get_hthread_manager()->thread_id_allocate();
    main_info.set_tid(main_gtid);

    ASSERT( main_gtid != L4_nilthread );
    
    if( !get_vm()->init_guest() ) {
	printf( "Unable to load guest module.\n");
	return false;
    }

    L4_ThreadId_t scheduler, pager;
    L4_Error_t last_error;
    L4_Word_t irq_prio;
    L4_MsgTag_t tag;
    L4_Msg_t ctrlxfer_msg;
    L4_Word64_t fault_id_mask;
    L4_Word_t fault_mask;
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    L4_Word_t mr = 1;

    scheduler = monitor_gtid;
    pager = monitor_gtid;
    // create thread
    last_error = ThreadControl( main_gtid, main_gtid, L4_Myself(), L4_nilthread, 0xeffff000 );
    if( last_error != L4_ErrOk )
    {
	printf( "Error: failure creating first thread, tid %t scheduler tid %d L4 error: %d\n",
		main_gtid, scheduler, last_error);
	return false;
    }
    
    // create address space
    last_error = SpaceControl( main_gtid, 1 << 30, L4_Fpage( 0xefffe000, 0x1000 ), 
			       L4_Fpage( 0xeffff000, 0x1000 ), L4_nilthread );
    
    if( last_error != L4_ErrOk )
    {
	printf( "Error: failure creating space, tid %t, L4 error code %d\n", main_gtid, last_error);
	goto err_space_control;
    }

    // set the thread's priority
    if( !L4_Set_Priority( main_gtid, get_vm()->get_prio() ) )
    {
	printf( "Error: failure setting guest's priority, tid %t, L4 error code %d\n", main_gtid, last_error);
	goto err_priority;
    }

    // make the thread valid
    last_error = ThreadControl( main_gtid, main_gtid, scheduler, pager, -1UL);
    if( last_error != L4_ErrOk ) {
	printf( "Error: failure starting thread guest's priority, tid %t, L4 error code %d\n", main_gtid, last_error);
	goto err_valid;
    }

    if( !backend_preboot( NULL ) ) {
	printf( "Error: backend_preboot failed\n");
	goto err_activate;
    }

    // start the thread
    L4_Set_Priority( main_gtid, 50 );
    printf( "Startup IP %x\n", get_vm()->entry_ip);
    
    if( get_vm()->guest_kernel_module == NULL ) 
	printf( "Starting in real mode\n");
    
    // cached cr0, see load_startup_reply
    if(get_vm()->guest_kernel_module == NULL)
	get_cpu().cr0.x.raw = 0x00000000;
    else
	get_cpu().cr0.x.raw = 0x00000031;
    

    main_info.mr_save.load_startup_reply( get_vm()->entry_ip, get_vm()->entry_sp, 
					  get_vm()->entry_cs, get_vm()->entry_ss,
					  (get_vm()->guest_kernel_module == NULL));
    main_info.mr_save.load();
    
    tag = L4_Send( main_gtid );
    if (L4_IpcFailed( tag ))
    {
	printf( "Error: failure sending startup IPC to %t\n", main_gtid);
	L4_KDB_Enter("failed");
	goto err_activate;
    }
        
    dprintf(debug_startup, "Main thread initialized TID %t VCPU %d\n", main_gtid, cpu_id);

    //by default, always send GPREGS, non regs and exc info
    fault_mask = L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_GPREGS_ID) | 
	L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_NONREGS_ID) | 
	L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_EXC_ID);
    fault_id_mask = ((1ULL << L4_CTRLXFER_HVM_FAULT(hvm_vmx_reason_max))-1) & ~0x1c3;
    
    L4_Clear(&ctrlxfer_msg);
    L4_AppendFaultConfCtrlXferItems(&ctrlxfer_msg, fault_id_mask, fault_mask);
    L4_Load(&ctrlxfer_msg);
    L4_ConfCtrlXferItems(main_gtid);

    //for io faults, also send ds/es info 
    fault_id_mask = L4_CTRLXFER_FAULT_ID_MASK(L4_CTRLXFER_HVM_FAULT(hvm_vmx_reason_io));
    fault_mask |= L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_DSREGS_ID) |
	L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_ESREGS_ID);
    L4_Clear(&ctrlxfer_msg);
    L4_AppendFaultConfCtrlXferItems(&ctrlxfer_msg, fault_id_mask, fault_mask);
    L4_Load(&ctrlxfer_msg);
    L4_ConfCtrlXferItems(main_gtid);
    
    //Read execution control fields
    L4_Set_MsgTag (L4_Niltag);
    vcpu_mrs->append_execctrl_item(L4_CTRLXFER_EXEC_PIN, 0);
    vcpu_mrs->append_execctrl_item(L4_CTRLXFER_EXEC_CPU, 0);
    vcpu_mrs->append_execctrl_item(L4_CTRLXFER_EXEC_EXC_BITMAP, 0);
    vcpu_mrs->load_execctrl_item();
    L4_ReadCtrlXferItems(main_gtid);
    vcpu_mrs->store_excecctrl_item(mr);
    dprintf(debug_startup, "VCPU execution control pin %x cpu %x exb_bmp %x\n", 
	   vcpu_mrs->execctrl_item.regs.pin,vcpu_mrs->execctrl_item.regs.cpu, 
	   vcpu_mrs->execctrl_item.regs.exc_bitmap);
    vcpu_mrs->dump(debug_startup, true);

    irq_prio = resourcemon_shared.prio + CONFIG_PRIO_DELTA_IRQ;
    irq_ltid = irq_init( irq_prio, L4_Pager(), this);
    if( L4_IsNilThread(irq_ltid) )
	return false;
    irq_gtid = L4_GlobalId( irq_ltid );
    irq_info.set_tid(irq_ltid);
    dprintf(debug_startup, "IRQ thread initialized TID %t VCPU %d\n", irq_gtid, cpu_id);
    
    return true;

 err_space_control:
 err_priority:
 err_valid:
 err_activate:
    
    // delete thread and space
    ThreadControl(main_gtid, L4_nilthread, L4_nilthread, L4_nilthread, -1UL);
    return false;
}
	
bool vcpu_t::handle_wedge_pfault(thread_info_t *ti, map_info_t &map_info, bool &nilmapping)
{
    const word_t wedge_paddr = get_wedge_paddr();
    const word_t wedge_end_paddr = get_wedge_end_paddr();
    vcpu_t &vcpu = get_vcpu();
    
    if (ti->get_tid() == vcpu.irq_gtid || ti->get_tid() == vcpu.irq_ltid)
    {
	idl4_fpage_t fp;
	CORBA_Environment ipc_env = idl4_default_environment;

	dprintf(debug_pfault, "Wedge pfault from IRQ thread %x\n", map_info.addr);
	idl4_set_rcv_window( &ipc_env, L4_CompleteAddressSpace );
	IResourcemon_pagefault( L4_Pager(), map_info.addr, map_info.addr, map_info.rwx, &fp, &ipc_env);
	nilmapping = true;
	return true;
    }	
    
    if ((map_info.addr >= wedge_paddr) && (map_info.addr < wedge_end_paddr))
    {
	printf( "Wedge (Phys) pfault, subsitute with scratch page %x\n", get_vm()->wedge_gphys);
	map_info.addr = get_vm()->wedge_gphys;
	return true;
    }
    return false;
	
}

bool vcpu_t::resolve_paddr(thread_info_t *ti, map_info_t &map_info, word_t &paddr, bool &nilmapping)
{
    paddr = map_info.addr;
    return false;
}


