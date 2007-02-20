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
#include INC_WEDGE(resourcemon.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(setup.h)
#include INC_WEDGE(monitor.h)
#include <device/acpi.h>
#include <device/apic.h>
#include <burn_symbols.h>

#if defined(CONFIG_DEVICE_APIC)
local_apic_t __attribute__((aligned(4096))) lapic VCPULOCAL("lapic");
acpi_t acpi;
#endif

vcpu_t vcpu VCPULOCAL("vcpu");

DECLARE_BURN_SYMBOL(vcpu);

void kdebug_putc( const char c )
{
    L4_KDB_PrintChar( c );
}

hconsole_t con;
hiostream_kdebug_t con_driver;
L4_KernelInterfacePage_t *kip;

char console_prefix[22];
inline void set_console_prefix()
{
    char conf_prefix[20] = CONFIG_CONSOLE_PREFIX;
    char *p = console_prefix;
    
    cmdline_key_search( "console_prefix=", conf_prefix, 20);
    
    for (word_t c = 0 ; c < 20 && conf_prefix[c] != 0 ; c++)
	*p++ = conf_prefix[c];
    
    char suffix[3] = ": ";
    for (word_t c = 0 ; c < 2 ; c++)
	*p++ = suffix[c];


}
void afterburn_main()
{
    
    kip = (L4_KernelInterfacePage_t *) L4_GetKernelInterface();
    vcpu_t::nr_vcpus = min((word_t) resourcemon_shared.vcpu_count, (word_t)  CONFIG_NR_VCPUS);
    vcpu_t::nr_pcpus =  min((word_t) resourcemon_shared.pcpu_count,
		  min ((word_t)  CONFIG_NR_CPUS, (word_t) L4_NumProcessors(L4_GetKernelInterface())));

    get_hthread_manager()->init( resourcemon_shared.thread_space_start,
	    resourcemon_shared.thread_space_len );
    
    get_vcpu(0).init_local_mappings();

    console_init( kdebug_putc,  console_prefix ); 
    printf( "Console initialized.\n" );

    for (word_t vcpu_id = 0; vcpu_id < vcpu_t::nr_vcpus; vcpu_id++)
	get_vcpu(vcpu_id).init(vcpu_id, L4_InternalFreq( L4_ProcDesc(kip, 0)));
    
    set_console_prefix();
    con_driver.init();
    con.init( &con_driver, console_prefix);
    



#if defined(CONFIG_DEVICE_APIC)
    acpi.init();
    word_t num_irqs = L4_ThreadIdSystemBase(kip);
#if defined(CONFIG_L4KA_VMEXT)
    num_irqs -= L4_NumProcessors(kip);
#endif
    get_intlogic().init_virtual_apics(num_irqs, vcpu_t::nr_vcpus);
    ASSERT(sizeof(local_apic_t) == 4096); 
#endif
    
    // Startup BSP VCPU
    if (!get_vcpu(0).startup_vm(resourcemon_shared.entry_ip, resourcemon_shared.entry_sp, 0, true))
	
    {
	con << "Couldn't start BSP VCPU VM\n";
	return;
    }
    
    con.enable_vcpu_prefix();

    // Enter the monitor loop.
    monitor_loop( vcpu, vcpu );
    
    
}

NORETURN void panic( void )
{
    while( 1 )
    {
	printf( "VM panic.  Halting VM threads.\n" );
	if( get_vcpu().main_ltid != L4_MyLocalId() )
	    L4_Stop( get_vcpu().main_ltid );
	if( get_vcpu().irq_ltid != L4_MyLocalId() )
	    L4_Stop( get_vcpu().irq_ltid );
	if( get_vcpu().monitor_ltid != L4_MyLocalId() )
	    L4_Stop( get_vcpu().monitor_ltid );
	L4_Stop( L4_MyLocalId() );
	L4_KDB_Enter( "VM panic" );
    }
}

