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
#include <console.h>
#include <debug.h>
#include <bind.h>
#include <burn_symbols.h>

#include <l4/schedule.h>
#include <l4/ipc.h>

#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(monitor.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(irq.h)

L4_Word_t max_hwirqs = 0;
virq_t virq VCPULOCAL("virq");

/**
 * called on the migration destination host to reinitialize the VCPU
 * 1. set the new monitor TID
 * 2. create new IRQ thread and start operation
 * 3. create and resume all other VM threads
 */
bool vcpu_t::resume_vcpu()
{
    L4_Word_t priority;

    // set the monitor TID in the new VM
    monitor_gtid = L4_Myself();
    monitor_ltid = L4_MyLocalId();

    // Create and start the new IRQ thread.
    priority = get_vcpu_max_prio() + CONFIG_PRIO_DELTA_IRQ;
    if (!irq_init(priority, L4_Pager(), this))
    {
	printf( "Failed to initialize IRQ thread for VCPU %d\n", cpu_id);
	return false;
    }
    dprintf(debug_startup, "IRQ thread initialized tid %t VCPU %d\n", irq_gtid, cpu_id);
    
    // create and resume all other threads
    get_thread_manager().resume_vm_threads();
    
    return false;
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
    L4_Set_Pager (resourcemon_shared.resourcemon_tid);
    
#if !defined(CONFIG_SMP_ONE_AS)
    // Initialize VCPU local data
    vcpu.init_local_mappings(vcpu.cpu_id);
#endif
    
    dprintf(debug_startup, "monitor thread's TID: %t boot VCPU %d startup IP %x SP %x\n",
	    L4_Myself(), boot_vcpu_id, startup_ip, startup_sp);

    // Flush complete address space, to get it remapped by resourcemon
    //L4_Flush( L4_CompleteAddressSpace + L4_FullyAccessible );
    vcpu.set_pcpu_id(0);
    
    // Startup AP VM
    vcpu.startup_vcpu(startup_ip, startup_sp, boot_vcpu_id, false);

    // Enter the monitor loop.
    get_vcpu(boot_vcpu_id).bootstrap_other_vcpu_done();

    monitor_loop(vcpu, get_vcpu(boot_vcpu_id) );
    
    printf( "PANIC, monitor fell through\n");       
    panic();
}

bool vcpu_t::startup(word_t vm_startup_ip)
{
    vcpu_t &boot_vcpu = get_vcpu();
    
    L4_Error_t errcode;
    // Create a monitor task
    monitor_gtid =  get_hthread_manager()->thread_id_allocate();
    if( L4_IsNilThread(monitor_gtid) )
	PANIC( "Failed to allocate monitor thread for VCPU %d : out of thread IDs.", 
		boot_vcpu.cpu_id );

    // Set monitor priority.
    L4_Word_t monitor_prio = get_vcpu_max_prio() + CONFIG_PRIO_DELTA_MONITOR;

    L4_Word_t utcb_area = afterburn_utcb_area;
    L4_ThreadId_t vcpu_space = monitor_gtid;
    L4_ThreadId_t pager = boot_vcpu.monitor_gtid;


#if defined(CONFIG_SMP_ONE_AS)
    vcpu_space = L4_Myself();
    utcb_area += 4096 * cpu_id;
    pager = resourcemon_shared.cpu[get_pcpu_id()].resourcemon_tid;
#endif

	    
    // Create the monitor thread.
    errcode = ThreadControl( 
	monitor_gtid,		   // monitor
	vcpu_space,		   // space
	boot_vcpu.main_gtid,	   // scheduler
	L4_nilthread,		   // pager
	monitor_prio,              // priority
	utcb_area		   // utcb_location
	);
	
    if( errcode != L4_ErrOk )
	PANIC( "Failed to create monitor thread for VCPU %d TID %t L4 error %s\n",
		boot_vcpu.cpu_id, monitor_gtid, L4_ErrString(errcode));

    L4_Fpage_t utcb_fp = L4_Fpage( utcb_area, CONFIG_UTCB_AREA_SIZE );
    L4_Fpage_t kip_fp = L4_Fpage( (L4_Word_t) afterburn_kip_area, CONFIG_KIP_AREA_SIZE);
    errcode = SpaceControl( monitor_gtid, 0, kip_fp, utcb_fp, L4_nilthread );
	
    if( errcode != L4_ErrOk )
	PANIC( "Failed to create monitor address space for VCPU %d TID %t L4 error %s\n",
		boot_vcpu.cpu_id, monitor_gtid, L4_ErrString(errcode));
	
   
    // Make the monitor thread valid.
    errcode = ThreadControl( 
	monitor_gtid,			// monitor
	monitor_gtid,			// new aS
	boot_vcpu.monitor_gtid,		// scheduler, for startup
	pager,
	utcb_area,
	monitor_prio);
    
    
    if( errcode != L4_ErrOk )
	PANIC( "Failed to make valid monitor address space for VCPU %d TID %t L4 error %s\n",
		boot_vcpu.cpu_id, monitor_gtid, L4_ErrString(errcode));

  
#if defined(CONFIG_L4KA_VMEXT)
    setup_thread_faults(monitor_gtid, true);
#endif
      
    word_t *vcpu_monitor_params = (word_t *) (afterburn_monitor_stack[cpu_id] + KB(16));

    vcpu_monitor_params[0]  = get_vcpu_stack();
    vcpu_monitor_params[-1] = vm_startup_ip;
    vcpu_monitor_params[-2] = boot_vcpu.cpu_id;
    vcpu_monitor_params[-3] = (word_t) this;
    
    word_t vcpu_monitor_sp = (word_t) vcpu_monitor_params;
    
    // Ensure that the monitor stack conforms to the function calling ABI.
    vcpu_monitor_sp = (vcpu_monitor_sp - CONFIG_STACK_SAFETY) & ~(CONFIG_STACK_ALIGN-1);
    
    dprintf(debug_startup, "starting up monitor %t VCPU %d IP %x SP  %x\n",
	    monitor_gtid, cpu_id, vcpu_monitor_thread, vcpu_monitor_sp);
    
   
    boot_vcpu.bootstrap_other_vcpu(cpu_id);	    
    monitor_info.mr_save.load_startup_reply(
	(L4_Word_t) vcpu_monitor_thread, (L4_Word_t) vcpu_monitor_sp);
    monitor_info.mr_save.set_propagated_reply(boot_vcpu.monitor_gtid); 	
    monitor_info.mr_save.load();
    boot_vcpu.main_info.mr_save.load_yield_msg(monitor_gtid);

    L4_MsgTag_t tag = L4_Send(monitor_gtid);


    while (is_off())
	L4_ThreadSwitch(monitor_gtid);

    if (!L4_IpcSucceeded(tag))
	PANIC( "Failed to activate monitor for VCPU %d TID %t L4 error %s\n",
		boot_vcpu.cpu_id, monitor_gtid, L4_ErrString(errcode));

    dprintf(debug_startup, "AP startup sequence for VCPU %d done\n.", cpu_id);
    
    return true;
}   

#endif  /* defined(CONFIG_VSMP) */ 


bool irq_init( L4_Word_t prio, L4_ThreadId_t pager_tid, vcpu_t *vcpu )
{
    L4_KernelInterfacePage_t *kip  = (L4_KernelInterfacePage_t *) L4_GetKernelInterface();
    IResourcemon_shared_cpu_t * rmon_cpu_shared;

    max_hwirqs = L4_ThreadIdSystemBase(kip) - L4_NumProcessors(kip);
    L4_Word_t pcpu_id = vcpu->get_pcpu_id();
    rmon_cpu_shared = &resourcemon_shared.cpu[pcpu_id];
    
    get_vcpulocal(virq).vtimer_irq = max_hwirqs + vcpu->cpu_id;
    get_vcpulocal(virq).bitmap = (bitmap_t<INTLOGIC_MAX_HWIRQS> *) resourcemon_shared.virq_pending;

    L4_ThreadId_t irq_tid;
    irq_tid.global.X.thread_no = max_hwirqs + vcpu->cpu_id;
    irq_tid.global.X.version = 1;
    
    dprintf(irq_dbg_level(INTLOGIC_TIMER_IRQ), "associating virtual timer irq %d handler %t\n", 
	    max_hwirqs + vcpu->cpu_id, L4_Myself());
   
    L4_Error_t errcode = AssociateInterrupt( irq_tid, L4_Myself(), 0, pcpu_id);
    
    if ( errcode != L4_ErrOk )
	printf( "Unable to associate virtual timer irq %d handler %t L4 error %s\n", 
		max_hwirqs + vcpu->cpu_id, L4_Myself(), L4_ErrString(errcode));

    /* Turn off ctrlxfer items */
    setup_thread_faults(L4_Myself(), false);

    dprintf(irq_dbg_level(INTLOGIC_TIMER_IRQ), "virtual timer %d virq tid %t\n", 
	    max_hwirqs + vcpu->cpu_id, vcpu->get_hwirq_tid());

    vcpu->irq_ltid = vcpu->monitor_ltid;
    vcpu->irq_gtid = vcpu->monitor_gtid;

    return true;
}

bool main_init( L4_Word_t prio, L4_ThreadId_t pager_tid, hthread_func_t start_func, vcpu_t *vcpu)
{
    L4_Word_t preemption_control;
    L4_Error_t errcode;
    
    hthread_t *main_thread = get_hthread_manager()->create_thread(
	vcpu,				// vcpu object
	vcpu->get_vcpu_stack_bottom(),	// stack bottom
	vcpu->get_vcpu_stack_size(),	// stack size
	prio,				// prio
	start_func,			// start func
	pager_tid,			// pager
	vcpu,				// start param
	NULL, 0);

    if( !main_thread )
    {
	printf( "Failed to initialize main thread for VCPU %d\n", vcpu->cpu_id);
	return false;
    }

    vcpu->main_ltid = main_thread->get_local_tid();
    vcpu->main_gtid = main_thread->get_global_tid();
    vcpu->init_info.entry_ip = main_thread->start_ip;
    vcpu->init_info.entry_sp = main_thread->start_sp;
    
    setup_thread_faults(vcpu->main_gtid, true);

    preemption_control = (vcpu->get_vcpu_max_prio() + CONFIG_PRIO_DELTA_IRQ << 16) | 2000;

    errcode  = ThreadControl( vcpu->main_gtid, vcpu->main_gtid, vcpu->monitor_gtid, L4_nilthread, (word_t) -1 );
    if (errcode != L4_ErrOk)
    {
	printf( "Error: unable to set main thread's scheduler %t L4 error: %s\n",
		vcpu->monitor_gtid, L4_ErrString(errcode));
	return false;
    }
    
#if defined(CONFIG_VSMP)
    bool mbt = vcpu->remove_vcpu_hthread(vcpu->main_gtid);
    ASSERT(mbt);
    vcpu->hthread_info.init();
#endif
    

    return true;
    
}


