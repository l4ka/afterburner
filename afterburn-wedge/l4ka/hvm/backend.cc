/*********************************************************************
 *                
 * Copyright (C) 2007-2008,  Karlsruhe University
 *                
 * File path:     backend.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/

#include <debug.h>
#include <l4/schedule.h>
#include <l4/ipc.h>
#include <string.h>
#include <elfsimple.h>
#include INC_WEDGE(vm.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(module_manager.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)

extern word_t afterburn_c_runtime_init;

extern unsigned char _binary_rombios_bin_start[];
extern unsigned char _binary_rombios_bin_end[];
extern unsigned char _binary_vgabios_bin_start[];
extern unsigned char _binary_vgabios_bin_end[];

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
    DEBUGGER_ENTER("UNTESTED RESOLV ADDR");
    
    pgent_t *pdir = (pgent_t*)(get_cpu().cr3.get_pdir_addr());
    pdir += pgent_t::get_pdir_idx(user_vaddr);

    if(!pdir->is_valid()) {
	printf( "Invalid pdir entry\n");
	return 0;
    }
    if(pdir->is_superpage()) 
	return pdir;

    pgent_t *ptab = (pgent_t*)pdir->get_address();
    ptab += pgent_t::get_ptab_idx(user_vaddr);

    if(!ptab->is_valid()) {
	printf( "Invalid ptab entry\n");
	return 0;
    }
    return ptab;

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



// signal to vcpu that we would like to deliver an interrupt
bool backend_async_irq_deliver( intlogic_t &intlogic )
{
    L4_Msg_t ctrlxfer_msg;
    word_t vector, irq;
    
    vcpu_t vcpu = get_vcpu();
    
    // are we already waiting for an interrupt window exit
    if( vcpu.interrupt_window_exit )
	return false;
    
    if( !intlogic.pending_vector( vector, irq ) )
	return false;

    // Get EFLAGS
    L4_RegCtrlXferItem_t eflags_item;
    L4_RegCtrlXferItemInit(&eflags_item, L4_CTRLXFER_GPREGS_ID);
    L4_RegCtrlXferItemSet(&eflags_item, L4_CTRLXFER_GPREGS_EFLAGS, 0);

    L4_Clear(&ctrlxfer_msg);
    L4_Append(&ctrlxfer_msg, &eflags_item);
    L4_Load(&ctrlxfer_msg);
    L4_ReadCtrlXferItems(vcpu.main_ltid);
    L4_Word_t eflags;
    L4_StoreMR(2, &eflags);


    if (eflags & 0x200)
    {
	//    L4_TBUF_RECORD_EVENT(12, "IL deliver irq immediately %d", irq);
	dprintf(irq_dbg_level(irq), "INTLOGIC deliver irq immediately %d\n", irq);
	printf("INTLOGIC deliver vector %d irq immediately %d\n", vector, irq);
	DEBUGGER_ENTER("XXX");
	backend_sync_deliver_vector( vector, false, false, false );

	return true;
    } 
    else 
    {
	intlogic.reraise_vector(vector, irq);
	//   L4_TBUF_RECORD_EVENT(12, "IL delay irq via window exit %d", irq);
	printf("INTLOGIC delay irq via window exit %d\n", irq);
	// inject interrupt request
	vcpu.interrupt_window_exit = true;
	
	L4_FaultInjectCtrlXferItem_t fault_item;
	L4_FaultInjectCtrlXferItemInit(&fault_item, 0);
	L4_Clear(&ctrlxfer_msg);
	L4_Append( &ctrlxfer_msg, &fault_item);
	L4_Load(&ctrlxfer_msg);
	L4_WriteCtrlXferItems(vcpu.main_ltid);
	return false;
    }
}


