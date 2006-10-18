/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
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

#include <l4/schedule.h>
#include <l4/ipc.h>

#include <bind.h>
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(setup.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(vm.h)
#include INC_WEDGE(irq.h)


static const bool debug_vcpu_startup=0;

typedef void (*vm_entry_t)();

L4_Word8_t vcpu_stacks[CONFIG_NR_VCPUS][vcpu_t::vcpu_stack_size] ALIGNED(CONFIG_STACK_ALIGN);

void vcpu_t::init_local_mappings( void ) 
{
    
    CORBA_Environment ipc_env = idl4_default_environment;
    idl4_fpage_t idl4_vcpufp;

    for (word_t vcpu_vaddr = start_vcpulocal; vcpu_vaddr < end_vcpulocal; vcpu_vaddr += PAGE_SIZE)
    {
	L4_Fpage_t vcpu_vfp, shadow_vcpu_pfp;
	word_t vcpu_paddr = vcpu_vaddr - get_wedge_vaddr() + get_wedge_paddr();
	word_t shadow_vcpu_paddr = (word_t) GET_ON_VCPU(cpu_id, word_t, vcpu_paddr);
	
	shadow_vcpu_pfp = L4_FpageLog2( shadow_vcpu_paddr, PAGE_BITS );
	if (debug_vcpu_startup)
	    con << "remapping cpulocal page " << (void *) shadow_vcpu_paddr 
		<< " -> " << (void *)vcpu_vaddr  
		<< "\n";
	
	vcpu_vfp = L4_FpageLog2( vcpu_vaddr, PAGE_BITS );
	L4_Flush(vcpu_vfp + L4_FullyAccessible);
	idl4_set_rcv_window( &ipc_env, vcpu_vfp);
	IResourcemon_request_pages( L4_Pager(), shadow_vcpu_pfp.raw, 7, &idl4_vcpufp, &ipc_env );

	if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	    CORBA_exception_free( &ipc_env );
	    panic();
	}
    }

    
}

void vcpu_t::init(word_t id, word_t hz, local_apic_t &vcpu_lapic)
{

    ASSERT(cpu_id < CONFIG_NR_VCPUS);

    magic[0] = 'V';
    magic[1] = 'C';
    magic[2] = 'P';
    magic[3] = 'U';
    magic[4] = 'V';
    magic[5] = '0';
    
    dispatch_ipc = false; 
    dispatch_ipc_nr = 0; 
    //startup_status = status_off; 

    cpu_id = id;
    cpu_hz = hz;
    
    monitor_gtid = L4_nilthread;
    monitor_ltid = L4_nilthread;
    irq_gtid = L4_nilthread;
    irq_ltid = L4_nilthread;
    main_gtid = L4_nilthread;
    main_ltid = L4_nilthread;
    
    wedge_vaddr_end = get_wedge_vaddr() + get_wedge_end_paddr() - 
	get_wedge_paddr() + (CONFIG_WEDGE_VIRT_BUBBLE_PAGES * PAGE_SIZE);

    vcpu_stack_bottom = (word_t) vcpu_stacks[cpu_id];
    vcpu_stack = (vcpu_stack_bottom + get_vcpu_stack_size() 
	    - CONFIG_STACK_SAFETY) & ~(CONFIG_STACK_ALIGN-1);
        
    resourcemon_shared.wedge_phys_size = 
	get_wedge_end_vaddr() - get_wedge_vaddr();
    resourcemon_shared.wedge_virt_size = resourcemon_shared.wedge_phys_size;


#if defined(CONFIG_WEDGE_STATIC)
    set_kernel_vaddr( resourcemon_shared.link_vaddr );
#else
    set_kernel_vaddr( 0 );
#endif

    if( !frontend_init(&cpu) )
    {
	con << "failed to initialize frontend\n";
	return;
    }

   
    vcpu_lapic.init(id, (id == 0));
    extern local_apic_t lapic;
    cpu.set_lapic(&lapic);
    ASSERT(sizeof(local_apic_t) == 4096); 

}

static void vcpu_main_thread( void *param, hthread_t *hthread )
{
    con << "Entering main VM thread, TID " << hthread->get_global_tid() << '\n';
    backend_vcpu_init_t *init_info = 
    	(backend_vcpu_init_t *)hthread->get_tlocal_data();

    // Set our thread's local CPU.  This wipes out the hthread tlocal data.
    vcpu_t *vcpu_param =  (vcpu_t *) param;
    set_vcpu(*vcpu_param);
    vcpu_t &vcpu = get_vcpu();
    ASSERT(vcpu.cpu_id == vcpu_param->cpu_id);
    
   
    // Set our thread's exception handler.
    L4_Set_ExceptionHandler( get_vcpu().monitor_gtid );

    vm_entry_t entry = (vm_entry_t) 0;
    if (init_info->vcpu_bsp)
    {   
#if defined(CONFIG_WEDGE_STATIC)
	// Minor runtime binding to the guest OS.
	afterburn_exit_hook = backend_exit_hook;
	afterburn_set_pte_hook = backend_set_pte_hook;
#else
	// Load the kernel into memory and rewrite its instructions.
	if( !backend_load_vcpu(init_info) )
	    panic();
#endif
    
	// Prepare the emulated CPU and environment.  NOTE: this function 
	// may start the VM and never return!!
	if( !backend_preboot(init_info) )
	    panic();
		
	entry = (vm_entry_t)init_info->entry_ip;

    }
    else
    {
	// Virtual APs boot in protected mode w/o paging
	get_vcpu().set_kernel_vaddr(get_vcpu(0).get_kernel_vaddr());
	get_vcpu().cpu.enable_protected_mode();
	entry = (vm_entry_t) (init_info->entry_ip - get_vcpu().get_kernel_vaddr()); 
	
    }    
    
    if (1 || debug_vcpu_startup)
	con << (init_info->vcpu_bsp ? " (BSP)" : " (AP)")
	    << " main thread, TID " << hthread->get_global_tid() 
	    << " ip " << (void *) init_info->entry_ip 
	    << " sp " << (void *) init_info->entry_sp 
	    << "\n";

    ASSERT(entry);    
    // Start executing the VM's binary if backend_preboot() didn't do so.
    entry();
}

bool vcpu_t::startup_vm(word_t startup_ip, word_t startup_sp, bool bsp)
{
    
    // Setup the per-CPU VM stack.
    ASSERT(cpu_id < CONFIG_NR_VCPUS);    
    // I'm the monitor
    monitor_gtid = L4_Myself();
    monitor_ltid = L4_MyLocalId();

    ASSERT(startup_ip);

    if (!startup_sp)
	startup_sp = get_vcpu_stack();

    // Create and start the IRQ thread.
    L4_Word_t irq_prio = resourcemon_shared.prio + CONFIG_PRIO_DELTA_IRQ_HANDLER;
    irq_ltid = irq_init(irq_prio, L4_Myself(), L4_Myself(), this);

    if( L4_IsNilThread(irq_ltid) )
    {
	con << "failed to initialize IRQ thread for VCPU " << cpu_id << "\n";
	return false;
    }

    irq_gtid = L4_GlobalId( irq_ltid );
    
    // Create the main VM thread.
    backend_vcpu_init_t init_info = 
	{ entry_sp : startup_sp, 
	  entry_ip :  startup_ip, 
	  vcpu_bsp : bsp
	};
    
    L4_Word_t main_prio = resourcemon_shared.prio + CONFIG_PRIO_DELTA_MAIN;

    hthread_t *main_thread = get_hthread_manager()->create_thread(
	get_vcpu_stack_bottom(),	// stack bottom
	get_vcpu_stack_size(),		// stack size
	main_prio,			// prio
	//get_pcpu_id(),		// processor number
	vcpu_main_thread,		// start func
	L4_Myself(),			// scheduler
	L4_Myself(),			// pager
	this,				// start param
	&init_info,			// thread local data
	sizeof(init_info)		// size of thread local data
	);

    if( !main_thread )
    {
	con << "failed to initialize main thread for VCPU " << cpu_id << "\n";
	return false;
    }

    main_ltid = main_thread->get_local_tid();
    main_gtid = main_thread->get_global_tid();
    
    // Setup priority relationships.
    if( !L4_Set_PreemptionDelay(main_ltid, irq_prio, 2000) )
	L4_KDB_Enter("preemption delay error"); 


#if defined(CONFIG_L4KA_VMEXTENSIONS)
    DEBUGGER_ENTER("");
    L4_Word_t dummy;
    L4_Word_t preemption_control = L4_PREEMPTION_CONTROL_MSG;
    L4_Word_t time_control = ~0UL;
    if (!L4_Schedule(main_gtid, time_control, ~0UL, ~0UL, preemption_control, &dummy))
	PANIC( "Failed to enable preemption msgs for main thread");
    

    L4_ThreadId_t scheduler_tid = irq_gtid;
#else
    L4_ThreadId_t scheduler_tid = main_gtid;
#endif
    
    L4_Error_t errcode = ThreadControl( main_gtid, main_gtid, scheduler_tid, L4_nilthread, (word_t) -1 );
    if (errcode != L4_ErrOk)
    {
	con << "Error: unable to set main thread's scheduler "
    	    << "L4 error: " << L4_ErrString(errcode) 
	    << "\n";
	return NULL;
    }
   
    main_thread->start();
    
    return true;
}   



