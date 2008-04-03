/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4ka/vcpu.cc
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
#include <l4/schedule.h>
#include <l4/ipc.h>

#include <bind.h>
#include <burn_symbols.h>
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(monitor.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(l4thread.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(irq.h)

word_t get_pcpu_id()
{
    return get_vcpu().get_pcpu_id();
}

bool cpu_lock_t::delayed_preemption VCPULOCAL("sync") = false;

void cpu_lock_t::init(const char *lock_name)
{ 
    cpulock.set(L4_nilthread, invalid_pcpu);
#if defined(L4KA_DEBUG_SYNC)
    this->cpulock.name = lock_name;
    //L4_KDB_PrintString("LOCK_INIT(");
    //for (word_t i = 0 ; i < 4; i++)
    //L4_KDB_PrintChar(this->cpulock.name[i]);
    //L4_KDB_PrintString(")\n");
#endif
}





typedef void (*vm_entry_t)();

static void vcpu_main_thread( void *param, l4thread_t *l4thread )
{
    // Set our thread's local CPU.  This wipes out the l4thread tlocal data.
    vcpu_t *vcpu_param =  (vcpu_t *) param;
    set_vcpu(*vcpu_param);
    vcpu_t &vcpu = get_vcpu();
    ASSERT(vcpu.cpu_id == vcpu_param->cpu_id);
    
    // Set our thread's exception handler.
    L4_Set_ExceptionHandler( vcpu.monitor_gtid );

    dprintf(debug_startup, "Entering main VM thread, TID %t\n", l4thread->get_global_tid());

    vm_entry_t entry = (vm_entry_t) 0;
    
    dprintf(debug_startup, "%s main thread %t cpu id %d\n", 
	    (vcpu.init_info.vcpu_bsp ? "BSP" : "AP"),
	    l4thread->get_global_tid(), vcpu.cpu_id);

    if (vcpu.init_info.vcpu_bsp)
    {   
	//resourcemon_init_complete();
	// Load the kernel into memory and rewrite its instructions.
	if( !backend_load_vcpu(vcpu) )
	    panic();
	// Prepare the emulated CPU and environment.  
	if( !backend_preboot(vcpu) )
	    panic();

#if defined(CONFIG_VSMP)
	vcpu.turn_on();
#endif
	dprintf(debug_startup, "main thread executing guest OS IP %x SP %x\n", 
		vcpu.init_info.entry_ip, vcpu.init_info.entry_sp);


	// Start executing the binary.
	__asm__ __volatile__ (
	    "movl %0, %%esp ;"
	    "push $0 ;" /* For FreeBSD. */
	    "push $0x1802 ;" /* Boot parameters for FreeBSD. */
	    "push $0 ;" /* For FreeBSD. */
	    "jmpl *%1 ;"
	    : /* outputs */
	    : /* inputs */
	      "a"(vcpu.init_info.entry_sp), "b"(vcpu.init_info.entry_ip),
	      "S"(vcpu.init_info.entry_param)
	    );

	
    }
    else
    {
	// Virtual APs boot in protected mode w/o paging
	vcpu.set_kernel_vaddr(get_vcpu(0).get_kernel_vaddr());
	vcpu.cpu.enable_protected_mode();
	entry = (vm_entry_t) (vcpu.init_info.entry_ip - vcpu.get_kernel_vaddr()); 
#if defined(CONFIG_VSMP)
	vcpu.turn_on();
#endif
	ASSERT(entry);    
	entry();
    }    
    
   
}

bool vcpu_t::startup_vcpu(word_t startup_ip, word_t startup_sp, word_t boot_id, bool bsp)
{
    
    L4_Word_t priority;
	
    // Setup the per-CPU VM stack.
    ASSERT(cpu_id < CONFIG_NR_VCPUS);    
    // I'm the monitor
    monitor_gtid = L4_Myself();
    monitor_ltid = L4_MyLocalId();

    ASSERT(startup_ip);
    if (!startup_sp) startup_sp = get_vcpu_stack();

#if defined(CONFIG_SMP)
    L4_Word_t num_pcpus = min((word_t) resourcemon_shared.pcpu_count, 
			      (word_t) L4_NumProcessors(L4_GetKernelInterface()));

    set_pcpu_id(cpu_id % num_pcpus);
    printf( "Set pcpu id to %d\n", cpu_id % num_pcpus, get_pcpu_id());
#endif
    
    // Create and start the IRQ thread.
    priority = get_vcpu_max_prio() + CONFIG_PRIO_DELTA_IRQ;
    
    if (!irq_init(priority, L4_Pager(), this))
    {
	printf( "Failed to initialize IRQ thread for VCPU %d\n", cpu_id);
	return false;
    }

    dprintf(debug_startup, "IRQ thread initialized tid %t VCPU %d\n", irq_gtid, cpu_id);
    
    priority = get_vcpu_max_prio() + CONFIG_PRIO_DELTA_MAIN;
    init_info.entry_param   = NULL; 
    init_info.vcpu_bsp      = bsp;

    if (!main_init(priority, L4_Myself(), vcpu_main_thread, this))
    {
	printf( "Failed to initialize main thread for VCPU %d\n", cpu_id);
	return false;
    }

    main_info.set_tid(main_gtid);
    main_info.mr_save.load_startup_reply(init_info.entry_ip, init_info.entry_sp);

    dprintf(debug_startup, "Main thread initialized tid %t VCPU %d entry ip %x\n", 
	    main_gtid, cpu_id, &init_info, init_info.entry_ip);

    init_info.entry_sp	    = startup_sp; 
    init_info.entry_ip      = startup_ip; 

    return true;

}   


word_t vcpu_t::nr_vcpus = CONFIG_NR_VCPUS;
vcpu_t vcpu VCPULOCAL("vcpu");
DECLARE_BURN_SYMBOL(vcpu);


L4_Word8_t vcpu_stacks[CONFIG_NR_VCPUS][vcpu_t::vcpu_stack_size] ALIGNED(CONFIG_STACK_ALIGN);

void vcpu_t::init_local_mappings( word_t id) 
{
    
    CORBA_Environment ipc_env = idl4_default_environment;
    idl4_fpage_t idl4_vcpufp;

    for (word_t vcpu_vaddr = start_vcpulocal; vcpu_vaddr < end_vcpulocal; vcpu_vaddr += PAGE_SIZE)
    {
	L4_Fpage_t vcpu_vfp, shadow_vcpu_pfp;
	word_t vcpu_paddr = vcpu_vaddr - get_wedge_vaddr() + get_wedge_paddr();
	word_t shadow_vcpu_paddr = (word_t) get_on_vcpu((word_t *) vcpu_paddr, id);
	
	shadow_vcpu_pfp = L4_FpageLog2( shadow_vcpu_paddr, PAGE_BITS );
	dprintf(debug_startup, "remapping cpulocal page %x -> %x\n", shadow_vcpu_paddr, vcpu_vaddr);
	
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

    ASSERT(id < CONFIG_NR_VCPUS);

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

    main_info.init();
    monitor_info.init();
    irq_info.init();
    
    wedge_vaddr_end = get_wedge_vaddr() + get_wedge_end_paddr() - 
	get_wedge_paddr() + (CONFIG_WEDGE_VIRT_BUBBLE_PAGES * PAGE_SIZE);

    vcpu_stack_bottom = (word_t) vcpu_stacks[cpu_id];
    vcpu_stack = (vcpu_stack_bottom + get_vcpu_stack_size() 
	    - CONFIG_STACK_SAFETY) & ~(CONFIG_STACK_ALIGN-1);
        
    resourcemon_shared.wedge_phys_size = 
	get_wedge_end_vaddr() - get_wedge_vaddr();
    resourcemon_shared.wedge_virt_size = resourcemon_shared.wedge_phys_size;

    set_kernel_vaddr( 0 );

    if( !frontend_init(&cpu) )
	PANIC("Failed to initialize frontend\n");


#if defined(CONFIG_DEVICE_APIC)
    extern local_apic_t lapic;
    local_apic_t &vcpu_lapic = *get_on_vcpu(&lapic, cpu_id);
    vcpu_lapic.init(id, (id == 0));
    
    /*
     * Get the APIC via VCPUlocal data
     */
    cpu.set_lapic(&lapic);
    ASSERT(sizeof(local_apic_t) == 4096); 
#endif
    
}


