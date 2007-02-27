/*********************************************************************
 *                
 * Copyright (C) 2007,  Karlsruhe University
 *                
 * File path:     vm.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/

#include <l4/schedule.h>
#include <l4/ipc.h>
#include <string.h>
#include <elfsimple.h>
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(vm.h)
#include INC_WEDGE(module_manager.h)

extern word_t afterburn_c_runtime_init;

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
    
    for( L4_Word_t idx = 0; idx < mm->get_module_count(); ++idx ) {
	module_t *module = mm->get_module( idx );
	ASSERT( module );
	
	bool valid_elf = elf_is_valid( module->vm_offset );
	if( guest_kernel_module == NULL && valid_elf == true ) {
	    guest_kernel_module = module;
	}
	else if( this->ramdisk_size == 0 && valid_elf == false ) {
	    ramdisk_start = module->vm_offset;
	    ramdisk_size = module->size;
	}
	
	if( guest_kernel_module != NULL && this->ramdisk_size > 0 ) {
	    break;
	}
    }
    
    if( ramdisk_size > 0 ) 
    {
	con << "Found ramdisk at " << (void*) this->ramdisk_start
	       << ", size " << (void*) this->ramdisk_size << ".\n";
    } 
    else 
    {
	con << "No ramdisk found.\n";
	return false;
    }

    return true;
}

bool vm_t::load_elf( L4_Word_t elf_start )
{
    word_t elf_base_addr, elf_end_addr;
    elf_ehdr_t *ehdr;
    word_t kernel_vaddr_offset;
    
    if( NULL == ( ehdr = elf_is_valid( elf_start ) ) ) {
	con << "Not a valid elf binary.\n";
	return false;
    }
    
    // install the binary in the first 64MB
    // convert to paddr
    if( ehdr->entry >= MB(64) )
	this->entry_ip = (MB(64)-1) & ehdr->entry;
    else
	this->entry_ip = ehdr->entry;
    
    // infer the guest's vaddr offset from its ELF entry address
    kernel_vaddr_offset = ehdr->entry - this->entry_ip;
    	    
    if( !ehdr->load_phys( kernel_vaddr_offset ) ) {
	con << "Elf loading failed.\n";
	return false;
    }

    ehdr->get_phys_max_min( kernel_vaddr_offset, elf_end_addr, elf_base_addr );

    con << "Summary: ELF Binary is residing at guest address ["
	   << (void*) elf_base_addr << " - "
	   << (void*) elf_end_addr << "] with entry point "
	   << (void*) this->entry_ip << ".\n";

    return true;
}



bool vm_t::init_guest( void )
{
    // check requested guest phys mem size
    if( this->guest_kernel_module != NULL ) {
	this->gphys_size = this->guest_kernel_module->get_module_param_size( "physmem=" );
    }
    
    if( this->gphys_size == 0 ) {
	con << "No physmem parameter given, using maximum available memory for guest phys mem.\n";
	this->gphys_size = resourcemon_shared.phys_offset;
    }
    
    // round to 4MB 
    this->gphys_size &= ~(MB(4)-1);
    
    con << (this->gphys_size >> 20) << "M of guest phys mem available.\n";
    
    if( this->gphys_size > (word_t) afterburn_c_runtime_init ) {
	con << "Make sure that the wedge is installed above the maximum guest physical address.\n";
	return false;
    }
     
    ASSERT( this->gphys_size > 0 );

    // move ramdsk to guest phys space if not already there,
    // or out of guest phys space if we are using it as a real disk
    if( this->ramdisk_size > 0 ) {
	if( this->guest_kernel_module == NULL || this->ramdisk_start + this->ramdisk_size >= this->gphys_size ) {
	    L4_Word_t newaddr = this->gphys_size - this->ramdisk_size - 1;
	    
	    // align
	    newaddr &= PAGE_MASK;
	    ASSERT( newaddr + this->ramdisk_size < this->gphys_size );
	    
	    // move
	    memmove( (void*) newaddr, (void*) this->ramdisk_start, this->ramdisk_size );
	    this->ramdisk_start = newaddr;
	}
	if( this->guest_kernel_module == NULL ) {
	    this->gphys_size = this->ramdisk_start;
	    con << "Reducing guest phys mem to " << (this->gphys_size >> 20) << "M for RAM disk.\n";
	}
    }
    
    // load the guest kernel module
    if( this->guest_kernel_module == NULL ) {
	if( this->ramdisk_size < 512 ) {
	    con << "No guest kernel module or RAM disk.\n";
	    return false;
	}
	// load the boot sector to the fixed location of 0x7c00
	this->entry_ip = 0x7c00;
	// workaround for a bug causing the first byte to be overwritten,
	// probably in resource monitor
	*((u8_t*) this->ramdisk_start) = 0xeb;
	memmove( (void*) this->entry_ip, (void*) this->ramdisk_start, 512 );
    } else {
	if( !this->load_elf( this->guest_kernel_module->vm_offset ) ) {
	    con << "Loading the guest kernel module failed.\n";
	    return false;
	}

	// save cmdline
	// TODO: really save it in case module gets overwritten somehow
	this->cmdline = (char*) this->guest_kernel_module->cmdline_options();
    }
    
    return true;
}



