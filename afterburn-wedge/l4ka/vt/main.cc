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

#include <l4/thread.h>
#include <l4/types.h>

#include INC_WEDGE(vt/vm.h)
#include INC_WEDGE(vt/module_manager.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(resourcemon.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(vt/irq.h)
#include INC_WEDGE(vt/monitor.h)

hconsole_t con;
hiostream_kdebug_t con_driver;

void afterburn_main()
{
    // init console
    con_driver.init();
    con.init( &con_driver, CONFIG_CONSOLE_PREFIX );

    get_hthread_manager()->init( resourcemon_shared.thread_space_start, resourcemon_shared.thread_space_len );

    if( !get_module_manager()->init() ) {
	con << "Module manager initialization failed.\n";
	goto err_leave;
    }
    
    if( !get_vm()->init( L4_Myself(), con_driver ) ) {
	con << "VM initialization failed.\n";
	goto err_leave;
    }
	    
    if( !get_vm()->init_guest() ) {
	con << "Unable to load guest module.\n";
    }

    // start vm
    if( !get_vm()->start_vm( L4_Myself() ) ) {
	con << "Unable to start VM.\n";
	goto err_leave;
    }
	
    
    
    // start irq thread
    {
    L4_Word_t irq_prio = resourcemon_shared.prio + CONFIG_PRIO_DELTA_IRQ_HANDLER;
    virt_vcpu_t &vcpu = get_vm()->get_vcpu();
    vcpu.irq_ltid = irq_init( irq_prio, L4_Myself(), L4_Myself(), &vcpu);
    if( L4_IsNilThread(vcpu.irq_ltid) )
	return;
    vcpu.irq_gtid = L4_GlobalId( vcpu.irq_ltid );
    con << "irq thread's TID: " << vcpu.irq_ltid << '\n';
    }
    
    // enter the monitor loop.
    monitor_loop();

err_leave:
    L4_KDB_Enter("l4ka-vt-vmm startup error");
}

NORETURN void panic( void )
{
    while( 1 )
    {
	con << "VM panic. Halting VM threads.\n";
	
	/* TODO: halt all VM threads */
	
	L4_Stop( L4_MyLocalId() );
	L4_KDB_Enter( "VM panic" );
    }
}
