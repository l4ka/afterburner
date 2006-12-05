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
#include INC_WEDGE(monitor.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(setup.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(user.h)
#include INC_WEDGE(irq.h)

word_t	cpu_lock_t::max_pcpus;
#if defined(L4KA_DEBUG_SYNC)
L4_Word_t cpu_lock_t::debug_pcpu_id;
L4_ThreadId_t cpu_lock_t::debug_tid;
cpu_lock_t *cpu_lock_t::debug_lock;
#endif

static const bool debug_vcpu_startup=1;

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

void vcpu_t::init(word_t id, word_t hz)
{

    ASSERT(cpu_id < CONFIG_NR_VCPUS);

    magic[0] = 'V';
    magic[1] = 'C';
    magic[2] = 'P';
    magic[3] = 'U';
    magic[4] = 'V';
    magic[5] = '0';
    
    dispatch_ipc = false; 
    idle_frame = NULL; 
#if defined(CONFIG_VSMP)
    startup_status = status_off; 
#endif

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
	con << "Failed to initialize frontend\n";
	return;
    }
#if defined(CONFIG_DEVICE_APIC)
    local_apic_t &vcpu_lapic = get_lapic(id);
    vcpu_lapic.init(id, (id == 0));
    
    
    /*
     * Get the APIC via VCPUlocal data
     */
    extern local_apic_t lapic;
    cpu.set_lapic(&lapic);
    ASSERT(sizeof(local_apic_t) == 4096); 
#endif
    
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
    L4_Set_ExceptionHandler( vcpu.monitor_gtid );

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
	vcpu.set_kernel_vaddr(get_vcpu(0).get_kernel_vaddr());
	vcpu.cpu.enable_protected_mode();
	entry = (vm_entry_t) (init_info->entry_ip - vcpu.get_kernel_vaddr()); 
	
    }    
    
    if (1 || debug_vcpu_startup)
	con << (init_info->vcpu_bsp ? " (BSP)" : " (AP)")
	    << " main thread, TID " << hthread->get_global_tid() 
	    << " ip " << (void *) init_info->entry_ip 
	    << " sp " << (void *) init_info->entry_sp 
	    << " boot id " << init_info->boot_id
	    << "\n";
    
    vcpu.turn_on();
    
    ASSERT(entry);    
    // Start executing the VM's binary if backend_preboot() didn't do so.
    entry();
}

bool vcpu_t::startup_vm(word_t startup_ip, word_t startup_sp, 
	word_t boot_id, bool bsp)
{
    
    L4_Word_t preemption_control, time_control, priority;
    L4_ThreadId_t scheduler;
    L4_Error_t errcode;    
    L4_Word_t dummy;
	
    // Setup the per-CPU VM stack.
    ASSERT(cpu_id < CONFIG_NR_VCPUS);    
    // I'm the monitor
    monitor_gtid = L4_Myself();
    monitor_ltid = L4_MyLocalId();

    ASSERT(startup_ip);

    if (!startup_sp)
	startup_sp = get_vcpu_stack();

#if 1 || defined(CONFIG_SMP)
    //pcpu_id = cpu_id % cpu_lock_t::max_pcpus;
    pcpu_id = 1;
    con << "monitor migrate to PCPU " << pcpu_id << "\n";
    if (L4_Set_ProcessorNo(L4_Myself(), pcpu_id) == 0)
	con << "migrating monitor to PCPU  " << pcpu_id
		    << " Failed, errcode " << L4_ErrorCode()
		    << "\n";
#endif

    // Create and start the IRQ thread.
    priority = get_vcpu_max_prio() + CONFIG_PRIO_DELTA_IRQ;
    irq_ltid = irq_init(priority, L4_Myself(), L4_Pager(), this);

    if( L4_IsNilThread(irq_ltid) )
    {
	con << "Failed to initialize IRQ thread for VCPU " << cpu_id << "\n";
	return false;
    }

    irq_gtid = L4_GlobalId( irq_ltid );
    if (debug_vcpu_startup)
	con << "IRQ thread initialized"
	    << " tid " << irq_gtid
	    << " VCPU " << cpu_id << "\n";
    
    // Create the main VM thread.
    backend_vcpu_init_t init_info = 
	{ entry_sp	: startup_sp, 
	  entry_ip      : startup_ip, 
	  boot_id	: boot_id,
	  vcpu_bsp      : bsp,
	};
    
    priority = get_vcpu_max_prio() + CONFIG_PRIO_DELTA_MAIN;

    hthread_t *main_thread = get_hthread_manager()->create_thread(
	get_vcpu_stack_bottom(),	// stack bottom
	get_vcpu_stack_size(),		// stack size
	priority,			// prio
	pcpu_id,			// processor number
	vcpu_main_thread,		// start func
	L4_Myself(),			// scheduler
	L4_Myself(),			// pager
	this,				// start param
	&init_info,			// thread local data
	sizeof(init_info)		// size of thread local data
	);

    if( !main_thread )
    {
	con << "Failed to initialize main thread for VCPU " << cpu_id << "\n";
	return false;
    }

    main_ltid = main_thread->get_local_tid();
    main_gtid = main_thread->get_global_tid();
    main_info.mr_save.load_startup_reply(
	(L4_Word_t) main_thread->start_ip, (L4_Word_t) main_thread->start_sp);
    
    preemption_control = (get_vcpu_max_prio() + CONFIG_PRIO_DELTA_IRQ << 16) | 2000;
#if defined(CONFIG_L4KA_VMEXTENSIONS)
    preemption_control |= L4_PREEMPTION_CONTROL_MSG;;
    time_control = (L4_Never.raw << 16) | L4_Never.raw;
    scheduler = monitor_gtid;
#else
    time_control = ~0UL;
    scheduler = main_gtid;
#endif
    if (!L4_Schedule(main_gtid, time_control, ~0UL, ~0UL, preemption_control, &dummy))
	PANIC( "Failed to set scheduling parameters for main thread");
    errcode = ThreadControl( main_gtid, main_gtid, scheduler, L4_nilthread, (word_t) -1 );
    if (errcode != L4_ErrOk)
    {
	con << "Error: unable to set main thread's scheduler "
    	    << "L4 error: " << L4_ErrString(errcode) 
	    << "\n";
	return false;
    }
   
    if (debug_vcpu_startup)
	con << "Main thread initialized"
	    << " tid " << main_gtid
	    << " VCPU " << cpu_id << "\n";
    return true;
}   



#if defined(CONFIG_VSMP)
extern "C" void NORETURN vcpu_monitor_thread(vcpu_t *vcpu_param, word_t boot_vcpu_id, 
	word_t startup_ip, word_t startup_sp)
{

    set_vcpu(*vcpu_param);
    vcpu_t &vcpu = get_vcpu();
    
    ASSERT(vcpu.cpu_id == vcpu_param->cpu_id);
    ASSERT(boot_vcpu_id < CONFIG_NR_VCPUS);

   
    ASSERT(get_vcpu(boot_vcpu_id).cpu_id == boot_vcpu_id);
 
    // Change Pager
    L4_Set_Pager (resourcemon_shared.cpu[L4_ProcessorNo()].resourcemon_tid);
    get_vcpu(boot_vcpu_id).bootstrap_other_vcpu_done();
    
#if !defined(CONFIG_SMP_ONE_AS)
    // Initialize VCPU local data
    vcpu.init_local_mappings();
#endif
    
    if (1 || debug_vcpu_startup)
	con << "monitor thread's TID: " << L4_Myself() 
	    << " boot VCPU " <<  boot_vcpu_id
	    << " startup VM ip " << (void *) startup_ip
	    << " sp " << (void *) startup_sp
	    << '\n';

    // Flush complete address space, to get it remapped by resourcemon
    //L4_Flush( L4_CompleteAddressSpace + L4_FullyAccessible );
    vcpu.pcpu_id = 0;
    

    // Startup AP VM
    vcpu.startup_vm(startup_ip, startup_sp, boot_vcpu_id, false);

    // Enter the monitor loop.
    monitor_loop( vcpu , get_vcpu(boot_vcpu_id) );
    
    con << "PANIC, monitor fell through\n";       
    panic();
}

bool vcpu_t::startup(word_t vm_startup_ip)
{
    vcpu_t &boot_vcpu = get_vcpu();
    
    L4_Error_t errcode;
    // Create a monitor task
    monitor_gtid =  get_hthread_manager()->thread_id_allocate();
    if( L4_IsNilThread(monitor_gtid) )
	PANIC( "Failed to allocate monitor thread"
		<< " for VCPU " << cpu_id 
		<< ": Out of thread IDs." );

    // Set monitor priority.
    L4_Word_t prio = get_vcpu_max_prio() + CONFIG_PRIO_DELTA_MONITOR;
    
    // Create the monitor thread.
    errcode = ThreadControl( 
	monitor_gtid,		   // monitor
	monitor_gtid,		   // new space
	boot_vcpu.main_gtid,	   // scheduler
	L4_nilthread,		   // pager
	afterburn_utcb_area,       // utcb_location
	prio                       // priority
	);
	
    if( errcode != L4_ErrOk )
	PANIC( "Failed to create monitor thread"
		<< " for VCPU " << cpu_id 
		<< " TID " << monitor_gtid 
		<< " L4 error: " << L4_ErrString(errcode) );

    // Create the monitor address space
    L4_Fpage_t utcb_fp = L4_Fpage( (L4_Word_t) afterburn_utcb_area, CONFIG_UTCB_AREA_SIZE );
    L4_Fpage_t kip_fp = L4_Fpage( (L4_Word_t) afterburn_kip_area, CONFIG_KIP_AREA_SIZE);

    errcode = SpaceControl( monitor_gtid, 0, kip_fp, utcb_fp, L4_nilthread );
	
    if( errcode != L4_ErrOk )
	PANIC( "Failed to create monitor address space" 
		<< " for VCPU " << cpu_id 
		<< " TID " << monitor_gtid  
		<< " L4 error: " << L4_ErrString(errcode) );
	
    L4_Word_t monitor_prio = get_vcpu_max_prio() + CONFIG_PRIO_DELTA_IRQ;
    
    // Make the monitor thread valid.
    errcode = ThreadControl( 
	monitor_gtid,			// monitor
	monitor_gtid,			// new aS
	monitor_gtid,			// scheduler
	boot_vcpu.monitor_gtid,		// pager, for activation msg
	afterburn_utcb_area,
	monitor_prio
	);
	
    if( errcode != L4_ErrOk )
	PANIC( "Failed to make valid monitor thread"
		<< " for VCPU " << cpu_id 
		<< " monitor TID " << monitor_gtid  
		<< " L4 error: " << L4_ErrString(errcode) );
    
    word_t *vcpu_monitor_params = (word_t *) (afterburn_monitor_stack[cpu_id] + KB(16));
    
    vcpu_monitor_params[0]  = get_vcpu_stack();
    vcpu_monitor_params[-1] = vm_startup_ip;
    vcpu_monitor_params[-2] = boot_vcpu.cpu_id;
    vcpu_monitor_params[-3] = (word_t) this;

    
    word_t vcpu_monitor_sp = (word_t) vcpu_monitor_params;
    
    // Ensure that the monitor stack conforms to the function calling ABI.
    vcpu_monitor_sp = (vcpu_monitor_sp - CONFIG_STACK_SAFETY) & ~(CONFIG_STACK_ALIGN-1);
    
    
    if (debug_vcpu_startup)
	con << "starting up monitor " << monitor_gtid 
	    << " VCPU " << cpu_id
	    << ", ip " << (void *) vcpu_monitor_thread
	    << ", sp " << (void *) vcpu_monitor_sp
	    << "\n";
    
   
    boot_vcpu.bootstrap_other_vcpu(cpu_id);	    
    monitor_info.mr_save.load_startup_reply(
	(L4_Word_t) vcpu_monitor_thread, (L4_Word_t) vcpu_monitor_sp);
    monitor_info.mr_save.set_propagated_reply(boot_vcpu.monitor_gtid); 	
    monitor_info.mr_save.load_mrs();
    
    L4_ThreadId_t from;
    boot_vcpu.main_info.mr_save.load_yield_msg(monitor_gtid);
    L4_MsgTag_t tag = L4_ReplyWait(monitor_gtid, &from);
    
    while (is_off())
	L4_ThreadSwitch(monitor_gtid);

    
    if (!L4_IpcSucceeded(tag))
	PANIC( "Failed to activate monitor thread " << monitor_gtid 
	       << " for VCPU " << cpu_id 
	       << " L4 error: " << L4_ErrString(errcode) );

   
    if (!L4_IpcSucceeded(tag))
	PANIC( "Failed to startup VCPU " << cpu_id 
		<< " monitor TID " << monitor_gtid
		<< " L4 error: " << L4_ErrString(errcode) );

    

    if (!L4_IpcSucceeded(tag))
    {
	PANIC( "Failed ack from VCPU " << cpu_id 
		<< " monitor TID " << monitor_gtid
		<< " L4 error: " << L4_ErrString(errcode) );
    }
    
    if (debug_vcpu_startup)
	con << "AP startup sequence for VCPU " << cpu_id
	    << " done.\n";
    
    return true;
}   

#endif  /* defined(CONFIG_VSMP) */ 
