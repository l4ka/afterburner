#include <l4/kip.h>
#include <l4/schedule.h>
#include <l4/message.h>
#include <elfsimple.h>

#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(vt/module_manager.h)
#include INC_WEDGE(vt/helpers.h)
#include INC_WEDGE(vt/string.h)

#include INC_WEDGE(vt/vm.h)
#include INC_WEDGE(vt/backend.h)

#include INC_WEDGE(vt/ia32.h)

vm_t vm;

extern word_t afterburn_c_runtime_init;

bool vm_t::init( L4_ThreadId_t monitor_tid, hiostream_driver_t &con_driver )
{
    // init console
    vm_con.init( &con_driver, "VM: " );

    // init vcpu
    L4_ThreadId_t tid = get_hthread_manager()->thread_id_allocate();
    ASSERT( tid != L4_nilthread );
    this->vcpu.init( tid, monitor_tid, this );
    
    // find first elf file among the modules, assume that it is the kernel
    // find first non elf file among the modules assume that it is a ramdisk
    this->guest_kernel_module = NULL;
    this->ramdisk_start = NULL;
    this->ramdisk_size = 0;
    
    module_manager_t *mm = get_module_manager();
    ASSERT( mm );
    
    for( L4_Word_t idx = 0; idx < mm->get_module_count(); ++idx ) {
	module_t *module = mm->get_module( idx );
	ASSERT( module );
	
	bool valid_elf = elf_is_valid( module->vm_offset );
	if( this->guest_kernel_module == NULL && valid_elf == true ) {
	    this->guest_kernel_module = module;
	}
	else if( this->ramdisk_size == 0 && valid_elf == false ) {
	    this->ramdisk_start = module->vm_offset;
	    this->ramdisk_size = module->size;
	}
	
	if( this->guest_kernel_module != NULL && this->ramdisk_size > 0 ) {
	    break;
	}
    }
    
    if( this->ramdisk_size > 0 ) {
	vm_con << "Found ramdisk at " << (void*) this->ramdisk_start
	       << ", size " << (void*) this->ramdisk_size << ".\n";
    } else {
	vm_con << "No ramdisk found.\n";
    }
    
    return true;
}


bool vm_t::init_guest( void )
{
    // check requested guest phys mem size
    if( this->guest_kernel_module != NULL ) {
	this->gphys_size = this->guest_kernel_module->get_module_param_size( "physmem=" );
    }
    
    if( this->gphys_size == 0 ) {
	vm_con << "No physmem parameter given, using maximum available memory for guest phys mem.\n";
	this->gphys_size = resourcemon_shared.phys_offset;
    }
    
    // round to 4MB 
    this->gphys_size &= ~(MB(4)-1);
    
    vm_con << (this->gphys_size >> 20) << "M of guest phys mem available.\n";
    
    if( this->gphys_size > (word_t) afterburn_c_runtime_init ) {
	vm_con << "Make sure that the wedge is installed above the maximum guest physical address.\n";
	return false;
    }
     
    ASSERT( this->gphys_size > 0 );

    
    // move ramdisk to guest phys space if not already there,
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
	    vm_con << "Reducing guest phys mem to " << (this->gphys_size >> 20) << "M for RAM disk.\n";
	}
    }
    
    // load the guest kernel module
    if( this->guest_kernel_module == NULL ) {
	if( this->ramdisk_size < 512 ) {
	    vm_con << "No guest kernel module or RAM disk.\n";
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
	    vm_con << "Loading the guest kernel module failed.\n";
	    return false;
	}

	// save cmdline
	// TODO: really save it in case module gets overwritten somehow
	this->cmdline = (char*) this->guest_kernel_module->cmdline_options();
    }
    
    return true;
}


bool vm_t::load_elf( L4_Word_t elf_start )
{
    word_t elf_base_addr, elf_end_addr;
    elf_ehdr_t *ehdr;
    word_t kernel_vaddr_offset;
    
    if( NULL == ( ehdr = elf_is_valid( elf_start ) ) ) {
	vm_con << "Not a valid elf binary.\n";
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
	vm_con << "Elf loading failed.\n";
	return false;
    }

    ehdr->get_phys_max_min( kernel_vaddr_offset, elf_end_addr, elf_base_addr );

    vm_con << "Summary: ELF Binary is residing at guest address ["
	   << (void*) elf_base_addr << " - "
	   << (void*) elf_end_addr << "] with entry point "
	   << (void*) this->entry_ip << ".\n";

    return true;
}

bool vm_t::start_vm( L4_ThreadId_t monitor )
{
    L4_ThreadId_t tid, scheduler, pager;
    L4_Error_t last_error;
    
    tid = vcpu.get_thread_id();
    ASSERT( tid != L4_nilthread );
    scheduler = L4_Myself();
    pager = monitor;
    
    vm_con << " VCPU: thread tid: " << tid << '\n';
    vm_con << " Pager: "<< pager << "\n";

    // create thread
    last_error = ThreadControl( tid, tid, L4_Myself(), L4_nilthread, 0xeffff000 );
    if( last_error != L4_ErrOk )
    {
	vm_con << "Error: failure creating first thread, tid " << tid
	       << ", scheduler tid " << scheduler
	       << ", L4 error code " << last_error << ".\n";
	return false;
    }
    vm_con << "Created new thread, tid " << tid << ".\n";
    
    // create address space
    last_error = SpaceControl( tid, 1 << 30, L4_Fpage( 0xefffe000, 0x1000 ), L4_Fpage( 0xeffff000, 0x1000 ), L4_nilthread );
    if( last_error != L4_ErrOk )
    {
	vm_con << "Error: failure creating space, tid " << tid
	       << ", L4 error code " << last_error << ".\n";
	goto err_space_control;
    }
    vm_con << "Created new address space, tid " << tid << ".\n";

    // set the thread's priority
    if( !L4_Set_Priority( tid, this->get_prio() ) )
    {
	vm_con << "Error: failure setting guest's priority, tid " << tid
	       << ", L4 error code " << last_error << ".\n";
	goto err_priority;
    }

    // make the thread valid
    last_error = ThreadControl( tid, tid, scheduler, pager, -1UL);
    if( last_error != L4_ErrOk ) {
	vm_con << "Error: failure starting thread, tid " << tid
	       << ", L4 error code " << last_error << ".\n";
	goto err_valid;
    }

    if( !backend_preboot( &vcpu ) ) {
	vm_con << "Error: backend_preboot failed\n";
	goto err_activate;
    }

    // start the thread
    if( !vcpu.send_startup_ipc( this->entry_ip, this->guest_kernel_module == NULL ) ) {
	vm_con << "Error: failure making the thread runnable, tid " << tid << ".\n";
	goto err_activate;
    }
    
    return true;

 err_space_control:
 err_priority:
 err_valid:
 err_activate:
    
    // delete thread and space
    ThreadControl(tid, L4_nilthread, L4_nilthread, L4_nilthread, -1UL);
    return false;
}


