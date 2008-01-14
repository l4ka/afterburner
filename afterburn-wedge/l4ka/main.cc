/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4ka/main.cc
 * Description:   The initialization logic for the L4Ka wedge.
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
#include <l4/kip.h>

#include INC_ARCH(cpu.h)
#include INC_WEDGE(vm.h)
#include INC_WEDGE(resourcemon.h)
#include <console.h>
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(monitor.h)
#include INC_WEDGE(module_manager.h)

#if defined(CONFIG_DEVICE_APIC)
#include <device/acpi.h>
#include <device/apic.h>
local_apic_t __attribute__((aligned(4096))) lapic VCPULOCAL("lapic");
acpi_t acpi;
#endif

#if defined(CONFIG_DEVICE_IDE)
#include <device/ide.h>
ide_t ide;
#endif


char console_prefix[39];
hiostream_kdebug_t con_driver;
vm_t vm;

INLINE void debug_putc( const char c )
{
    con_driver.print_char(c);
}



inline void set_console_prefix()
{
    char conf_prefix[20] = CONFIG_CONSOLE_PREFIX;
    char *p = console_prefix;
    
    cmdline_key_search( "console_prefix=", conf_prefix, 20);
    
    char prefix[11] = "\e[2m\e[35m";
    for (word_t c = 0 ; c < 11 ; c++)
	*p++ = prefix[c];

    
    for (word_t c = 0 ; c < 20 && conf_prefix[c] != 0 ; c++)
	*p++ = conf_prefix[c];
    
    char suffix[7] = ":\e[0m ";
    for (word_t c = 0 ; c < 7 ; c++)
	*p++ = suffix[c];
}
void afterburn_main()
{
    L4_KernelInterfacePage_t *kip  = (L4_KernelInterfacePage_t *) L4_GetKernelInterface();
    vcpu_t::nr_vcpus = min((word_t) resourcemon_shared.vcpu_count, (word_t)  CONFIG_NR_VCPUS);

    get_hthread_manager()->init( resourcemon_shared.thread_space_start,
	    resourcemon_shared.thread_space_len );

    
    extern vcpu_t vcpu;
#if !defined(CONFIG_SMP_ONE_AS)
    vcpu.init_local_mappings(0);
    vcpu.init(0, L4_InternalFreq( L4_ProcDesc(kip, 0)));
#else
    vcpu_t &myvcpu = *get_on_vcpu(&vcpu, 0);  
    L4_Set_UserDefinedHandle((L4_Word_t) &myvcpu);
    myvcpu.init(0, L4_InternalFreq( L4_ProcDesc(kip, 0)));
#endif
    
    set_console_prefix();
    con_driver.init();
    console_init( debug_putc, console_prefix, true); 
    printf( "Console (printf) initialized.\n" );

    printf( "Afterburner supports up to %d vcpus\n", vcpu_t::nr_vcpus);
    
    for (word_t vcpu_id = 1; vcpu_id < vcpu_t::nr_vcpus; vcpu_id++)
	get_vcpu(vcpu_id).init(vcpu_id, L4_InternalFreq( L4_ProcDesc(kip, 0)));
    
    
#if defined(CONFIG_DEVICE_APIC)
    acpi.init();
    word_t num_irqs = L4_ThreadIdSystemBase(kip);
#if defined(CONFIG_L4KA_VMEXT)
    num_irqs -= L4_NumProcessors(kip);
#endif
    get_intlogic().init_virtual_apics(num_irqs, vcpu_t::nr_vcpus);
    ASSERT(sizeof(local_apic_t) == 4096); 
#endif    
  
    if( !get_module_manager()->init() ) 
     {
	printf( "Module manager initialization failed.\n");
	return;
    }
    
    if( !get_vm()->init(resourcemon_shared.entry_ip, resourcemon_shared.entry_sp))
    { 
	printf( "VM initialization failed.\n");
    }
       
#if 0
    // Migration HACK:
    // call the cloneVM interface of the resourcemonitor
    // ATM: only the monitor thread (L4_Myself()) is active
    // the cloneVM call creates a new VM, copies the memory,
    // creates a new monitor thread, sets the IP of the new
    // thread to resume_vm() and activates the monitor thread
    L4_ThreadId_t monitor_tid = L4_Myself();
    resourcemon_clone_vm(monitor_tid, (L4_Word_t)resume_vm);
    printf( "VM parent resumes execution\n");
#endif
   

    // Startup BSP VCPU
    if (!get_vcpu(0).startup_vcpu(get_vm()->entry_ip, get_vm()->entry_sp, 0, true))
	
    {
	printf( "Couldn't start BSP VCPU VM\n");
	return;
    }

#if defined(CONFIG_DEVICE_IDE)
    ide.init();
#endif

    // Enter the monitor loop.
    monitor_loop( get_vcpu(), get_vcpu() );
    
    
}

/* main returns here after the fork in afterburn_main */
void resume_vm(void)
{
    L4_KernelInterfacePage_t *kip  = (L4_KernelInterfacePage_t *) L4_GetKernelInterface();
    vcpu_t::nr_vcpus = min((word_t) resourcemon_shared.vcpu_count, (word_t)  CONFIG_NR_VCPUS);

    // init the new thread space
    get_hthread_manager()->init( resourcemon_shared.thread_space_start,
				 resourcemon_shared.thread_space_len );
    
    extern vcpu_t vcpu;
#if !defined(CONFIG_SMP_ONE_AS)
    vcpu.init_local_mappings(0);
    vcpu.init(0, L4_InternalFreq( L4_ProcDesc(kip, 0)));
#else
    vcpu_t &myvcpu = *get_on_vcpu(&vcpu, 0);  
    L4_Set_UserDefinedHandle((L4_Word_t) &myvcpu);
    myvcpu.init(0, L4_InternalFreq( L4_ProcDesc(kip, 0)));
#endif

    set_console_prefix();
    console_init( debug_putc, console_prefix); 
    printf( "Console (printf) initialized.\n" );
    
   
    for (word_t vcpu_id = 1; vcpu_id < vcpu_t::nr_vcpus; vcpu_id++)
	get_vcpu(vcpu_id).init(vcpu_id, L4_InternalFreq( L4_ProcDesc(kip, 0)));
    

#if 0 // FIXME        
#if defined(CONFIG_DEVICE_APIC)
    acpi.init();
    word_t num_irqs = L4_ThreadIdSystemBase(kip);
#if defined(CONFIG_L4KA_VMEXT)
    num_irqs -= L4_NumProcessors(kip);
#endif
    get_intlogic().init_virtual_apics(num_irqs, vcpu_t::nr_vcpus);
    ASSERT(sizeof(local_apic_t) == 4096); 
#endif // CONFIG_DEVICE_APIC
#endif

    // resourcemon_shared.modules
    if( !get_module_manager()->init() ) 
     {
	printf( "Module manager initialization failed.\n");
	return;
    }
    
    // resourcemont_shared.modules[0].cmdline
    if( !get_vm()->init(resourcemon_shared.entry_ip, resourcemon_shared.entry_sp))
    { 
	printf( "VM initialization failed.\n");
    }

#if defined(CONFIG_L4KA_VMEXT)    
    printf( "Afterburner resume vm %t\n", L4_Myself());

    printf( "resume vcpu 0\n");
    // resume vcpu
    if (!get_vcpu(0).resume_vcpu())
    {
	printf( "could not resume VCPU\n");
	return;
    }
#endif

#if 0
    // Startup BSP VCPU
    if (!get_vcpu(0).startup_vcpu(get_vm()->entry_ip,
				  get_vm()->entry_sp, 0, true))
    {
	printf( "Couldn't start BSP VCPU VM\n");
	return;
    }
#endif

    printf( "resumed VM enters the monitor loop\n");

    // Enter the monitor loop.
    monitor_loop( get_vcpu(), get_vcpu() );
}


NORETURN void panic( void )
{
    while( 1 )
    {
	L4_KDB_PrintString( "VM panic.  Halting VM threads.\n" );
	DEBUGGER_ENTER( "VM panic" );
	if( get_vcpu().main_ltid != L4_MyLocalId() )
	    L4_Stop( get_vcpu().main_ltid );
	if( get_vcpu().irq_ltid != L4_MyLocalId() )
	    L4_Stop( get_vcpu().irq_ltid );
	if( get_vcpu().monitor_ltid != L4_MyLocalId() )
	    L4_Stop( get_vcpu().monitor_ltid );
	L4_Stop( L4_MyLocalId() );
	DEBUGGER_ENTER( "VM panic" );
    }
}

