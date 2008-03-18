/*********************************************************************
 *                
 * Copyright (C) 2008,  Karlsruhe University
 *                
 * File path:     vcpu.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
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
    
    preemption_control = (vcpu->get_vcpu_max_prio() + CONFIG_PRIO_DELTA_IRQ << 16) | 2000;
    errcode  = ThreadControl( vcpu->main_gtid, vcpu->main_gtid, vcpu->main_gtid, L4_nilthread, (word_t) -1 );
    if (errcode != L4_ErrOk)
    {
	printf( "Error: unable to set main thread's scheduler %t L4 error: %s\n",
		vcpu->main_gtid, L4_ErrString(errcode));
	return false;
    }
    return true;
    
}
