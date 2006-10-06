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
#include <l4-common/hthread.h>
#include <l4-common/irq.h>
#include <l4-common/setup.h>
#include <l4-common/monitor.h>
#include <device/acpi.h>
#include <device/apic.h>
#include <burn_symbols.h>

hconsole_t con;
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

void afterburn_main()
{
    hiostream_kdebug_t con_driver;
    con_driver.init();

    con.init( &con_driver, CONFIG_CONSOLE_PREFIX ": " );
    console_init( kdebug_putc, "\e[1m\e[37m" CONFIG_CONSOLE_PREFIX ":\e[0m " );
    printf( "Console initialized.\n" );

    get_hthread_manager()->init( resourcemon_shared.thread_space_start,
	    resourcemon_shared.thread_space_len );
    
    get_vcpu(0).init_local_mappings();
    
    for (word_t vcpu_id = 0; vcpu_id < CONFIG_NR_VCPUS; vcpu_id++)
    {
	
	/*
	 * Circumvent get_lapic, since initialization is not ready before
	 * initialization
	 */
	
	local_apic_t &vcpu_lapic = get_lapic(vcpu_id);
	
	con << "Initialize VCPU " << vcpu_id
	    << " @ " << (void*) &get_vcpu(vcpu_id)
	    << "\n";
	
	get_vcpu(vcpu_id).init(vcpu_id, L4_InternalFreq( L4_ProcDesc(L4_GetKernelInterface(), 0)), vcpu_lapic);
    }

    acpi.init();
    get_intlogic().init_virtual_apics(L4_ThreadIdSystemBase(L4_GetKernelInterface()));
    ASSERT(sizeof(local_apic_t) == 4096); 
    
    // Startup BSP VCPU
    if (!get_vcpu(0).startup_vm(resourcemon_shared.entry_ip, resourcemon_shared.entry_sp, true))
	
    {
	con << "Couldn't start BSP VCPU VM\n";
	return;
    }

    // Enter the monitor loop.
    monitor_loop( vcpu );
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

