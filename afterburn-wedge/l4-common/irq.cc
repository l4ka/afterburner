/* (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4-common/irq.cc
 * Description:   The irq thread for handling asynchronous events.
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
 * $Id: irq.cc,v 1.28 2006/01/11 19:01:17 stoess Exp $
 *
 ********************************************************************/

#include <l4/ipc.h>
#include <l4/kip.h>
#include <l4/schedule.h>

#include INC_ARCH(intlogic.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(l4privileged.h)
#include <device/acpi.h>

#include <l4-common/hthread.h>
#include <l4-common/user.h>
#include <l4-common/irq.h>
#include <l4-common/message.h>

static const bool debug_hwirq=0;
static const bool debug_timer=0;
static const bool debug_virq=0;
static const bool debug_ipi=0;

static unsigned char irq_stack[CONFIG_NR_VCPUS][KB(16)] ALIGNED(CONFIG_STACK_ALIGN);
static const L4_Clock_t timer_length = {raw: 10000};

static void irq_handler_thread( void *param, hthread_t *hthread )
{
    L4_Word_t tid_user_base = L4_ThreadIdUserBase(L4_GetKernelInterface());
    L4_Word_t tid_system_base = L4_ThreadIdSystemBase (L4_GetKernelInterface());

    //vcpu_t *vcpu_param =  (vcpu_t *) param;
    //set_vcpu(*vcpu_param);
    vcpu_t &vcpu = get_vcpu();
    //ASSERT(vcpu.cpu_id == vcpu_param->cpu_id);
    
    con << "IRQ thread, "
	<< "TID " << hthread->get_global_tid() 
	<< "\n";      

    // Set our thread's exception handler. 
    L4_Set_ExceptionHandler( get_vcpu().monitor_gtid );

    L4_ThreadId_t tid = L4_nilthread;
    L4_ThreadId_t ack_tid = L4_nilthread, save_ack_tid = L4_nilthread;
    L4_Clock_t last_time = {raw: 0}, current_time = {raw: 0};
    L4_Word_t timer_irq = INTLOGIC_TIMER_IRQ;
    L4_Time_t periodic;
    bool do_timer = false, dispatch_ipc = false, was_dispatch_ipc = false;
    word_t reraise_irq = INTLOGIC_INVALID_IRQ, reraise_vector = 0;
    intlogic_t &intlogic = get_intlogic();
    local_apic_t &lapic = get_lapic();
    word_t dispatch_ipc_nr = 0;
    
    periodic = L4_TimePeriod( timer_length.raw );

    for (;;)
    {
	L4_MsgTag_t tag = L4_ReplyWait_Timeout( ack_tid, periodic, &tid );
	do_timer = false;
	save_ack_tid = ack_tid;
	ack_tid = L4_nilthread;
	was_dispatch_ipc = dispatch_ipc;
	dispatch_ipc = false;

	if( L4_IpcFailed(tag) )
	{
	    L4_Word_t err = L4_ErrorCode();

	    if( (err & 0xf) == 3 ) { // Receive timeout.
		// Timer interrupt.
		do_timer = true;
	    }
	    else if( (err & 0xf) == 2 ) { // Send timeout.
		if( was_dispatch_ipc )
		{
		    // User-level programs already in the send queue of the main
		    // thread can beat us to IPC delivery, causing
		    // us to time-out when sending a vector request to the
		    // dispatch loop.
		    if (debug_hwirq 
			    || intlogic.is_irq_traced(reraise_irq) 
			    || lapic.is_vector_traced(reraise_vector))
			con << "Reraise vector " << reraise_vector 
			    << " irq " << reraise_irq
			    << "\n"; 
		    intlogic.reraise_vector(reraise_vector, reraise_irq);
		}
		else
		{
		    if (ack_tid.global.X.thread_no >= tid_system_base || debug_hwirq)
		    {
			con << "IRQ thread send timeout"
			    << "to thread" << save_ack_tid 
			    << " error " << (void *) err
			    << "\n";
			//L4_KDB_Enter("BUG");
		    }
		}
		continue;
	    }
	    else {
		L4_KDB_Enter("IRQ IPC failure");
		continue;
	    }
	} /* IPC timeout */
	// Received message.
	else if( tid.global.X.thread_no < tid_user_base ) {
	    // Hardware IRQ.
	    L4_Word_t irq = tid.global.X.thread_no;
	    if(debug_hwirq || intlogic.is_irq_traced(irq)) 
		con << "hardware irq: " << irq
		    << ", int flag: " << get_cpu().interrupts_enabled()
		    << '\n';
	    
#if defined(CONFIG_DEVICE_PASSTHRU)
	    /* 
	     * jsXXX: not strictly necessary here, pic/apic will set it also
	     */
	    intlogic.set_hwirq_mask(irq);
	    intlogic.raise_irq( irq );
#else
	    L4_KDB_Enter("hardware irq");
#endif
	}
	else {
	    // Virtual interrupt from external source.
	    if( msg_is_virq(tag) ) {
		L4_Word_t irq;
		msg_virq_extract( &irq );
		if( debug_virq )
		    con << "virtual irq: " << irq 
			<< ", from TID " << tid << '\n';
		intlogic.raise_irq( irq );
	    }
	    else if( msg_is_device_ack(tag) ) {
		L4_Word_t irq;
		msg_device_ack_extract( &irq );
		if(debug_hwirq || intlogic.is_irq_traced(irq))
		    con << "hardware irq ack " << irq << '\n';
#if defined(CONFIG_DEVICE_PASSTHRU)
		// Send an ack message to the L4 interrupt thread.
		// TODO: the main thread should be able to do this via
		// propagated IPC.
		ack_tid.global.X.thread_no = irq;
		ack_tid.global.X.version = 1;
		L4_LoadMR( 0, 0 );  // Ack msg.

#else
		L4_KDB_Enter("irq ack");
#endif
	    }
	    else if( msg_is_ipi(tag) ) {
		L4_Word_t vector, src_vcpu_id;		
		msg_ipi_extract( &src_vcpu_id, &vector  );
		if (debug_ipi) 
		    con << " IPI from VCPU " << src_vcpu_id 
			<< " vector " << vector
			<< '\n';
		lapic.raise_vector(vector, INTLOGIC_INVALID_IRQ);;

	    }
#if defined(CONFIG_DEVICE_PASSTHRU)
	    else if( msg_is_device_enable(tag) ) {
		L4_Word_t irq;
		L4_Error_t errcode;
		L4_Word_t prio = resourcemon_shared.prio + CONFIG_PRIO_DELTA_IRQ;
		msg_device_enable_extract(&irq);
		tid.global.X.thread_no = irq;
		tid.global.X.version = 1;
		errcode = AssociateInterrupt( tid, L4_Myself() );
	
		if(debug_hwirq || intlogic.is_irq_traced(irq)) 
		    con << "enable device irq: " << irq << '\n';

		if( errcode != L4_ErrOk )
		    con << "Attempt to associate an unavailable interrupt: "
			<< irq << ", L4 error: " 
			<< L4_ErrString(errcode) << ".\n";
		else {
		    
		    if( !L4_Set_Priority(tid, prio) )
		    {
			con << "Unable to set irq priority"
			    << " irq " << irq 
			    << " to " << prio
			    << " ErrCode " << L4_ErrorCode() << "\n";
			L4_KDB_Enter("Error");
		    }
#if 0
		    con << "migrating IRQ " << irq
			<< " TID " << tid
			<< " to PCPU  " << vcpu.pcpu_id << "\n"; 

		    if (L4_Set_ProcessorNo(tid, vcpu.pcpu_id) == 0)
		    {
			con << "migrating IRQ to PCPU  " << vcpu.pcpu_id
			    << " failed, errcode " << L4_ErrorCode()
			    << "\n";
			DEBUGGER_ENTER(0);
		    }
		    con << "migrated IRQ " << irq
			<< " TID " << tid
			<< " to PCPU  " << vcpu.pcpu_id << "\n"; 
		    //if (vcpu.pcpu_id)
		    //DEBUGGER_ENTER(0);
#endif
		}

		
	    }
	    else if( msg_is_device_disable(tag) ) {
		L4_Word_t irq;
		L4_Error_t errcode;
		msg_device_disable_extract(&irq);
		tid.global.X.thread_no = irq;
		tid.global.X.version = 1;
		
		if(debug_hwirq || intlogic.is_irq_traced(irq)) 
		    con << "disable device irq: " << irq
			<< '\n';
		errcode = DeassociateInterrupt( tid );
		if( errcode != L4_ErrOk )
		    con << "Attempt to deassociate an unavailable interrupt: "
			<< irq << ", L4 error: " 
			<< L4_ErrString(errcode) << ".\n";
	    }
#endif /* defined(CONFIG_DEVICE_PASSTHRU) */
#if defined(CONFIG_VSMP)
	    else if( msg_is_migrate_uthread(tag) ) {
		L4_ThreadId_t uthread;
		L4_Word_t dest_vcpu_id, expected_control;		
 		msg_migrate_uthread_extract( &uthread, &dest_vcpu_id, &expected_control);
		if (debug_ipi) 
		    con << "request from VCPU " << dest_vcpu_id 
			<< " to migrate uthread " << uthread
			<< " expected control " << expected_control
			<< '\n';

		L4_Word_t old_control, old_sp, old_ip, old_flags;
		thread_manager_t::get_thread_manager().migrate(uthread, dest_vcpu_id, expected_control,
			&old_control, &old_sp, &old_ip, &old_flags );
		ack_tid = get_vcpu(dest_vcpu_id).main_gtid;
		msg_migrate_uthread_ack_build(uthread, old_control, old_sp, old_ip, old_flags);
	    }
	    else if( msg_is_signal_uthread(tag) ) {
		L4_ThreadId_t uthread;
		L4_Word_t dest_vcpu_id;
		msg_signal_uthread_extract( &uthread, &dest_vcpu_id);
		if (debug_ipi) 
		    con << "request from VCPU " << dest_vcpu_id 
			<< " to signal uthread " << uthread
			<< '\n';

		L4_Word_t old_control;
		thread_manager_t::get_thread_manager().signal(uthread, &old_control );
		ack_tid = get_vcpu(dest_vcpu_id).main_gtid;
		msg_signal_uthread_ack_build(uthread, old_control);
	    }
#endif
	    else
		con << "unexpected IRQ message from " << tid << '\n';
	    
	    continue;  // Don't attempt other interrupt processing.
	}

	// Make sure that we deliver our timer interrupts too!
	current_time = L4_SystemClock();
	if( !do_timer && ((current_time - timer_length) > last_time) )
	    do_timer = true;

	if( do_timer ) {
	    last_time = current_time;

	    if(debug_timer ||  intlogic.is_irq_traced(timer_irq))
		con << ", timer irq " << timer_irq 
		    << ", if: " << get_cpu().interrupts_enabled()
		    << "\n";
	    intlogic.raise_irq( timer_irq );
	}
	
	if( vcpu.in_dispatch_ipc() )
	{
	    word_t new_dispatch_nr = vcpu.get_dispatch_ipc_nr();
	    
	    //word_t int_save = vcpu.cpu.disable_interrupts();
	    //if (!int_save)
	    //continue;
	    
	    ASSERT(vcpu.cpu.interrupts_enabled());
	    
	    word_t vector, irq;
	    
	    if( !intlogic.pending_vector(vector, irq) )
	    {
		vcpu.cpu.restore_interrupts(true);
		continue;
	    }
	    
	    dispatch_ipc = true;
	    reraise_irq = irq;
	    reraise_vector = vector;
	    // Interrupts are enabled if we are in dispatch IPC.
	    dispatch_ipc_nr = new_dispatch_nr;
	    msg_vector_build( vector );
	    ack_tid = vcpu.main_gtid;
	    if(intlogic.is_irq_traced(irq) || lapic.is_vector_traced(vector))
		con << " forward IRQ " << irq 
		    << " vector " << vector
		    << " via IPC to idle VM (TID " << ack_tid << ")\n";
	}
	else
	    backend_async_irq_deliver( intlogic );

    } /* while */
}

L4_ThreadId_t irq_init( L4_Word_t prio, 
	L4_ThreadId_t scheduler_tid, L4_ThreadId_t pager_tid,
	vcpu_t *vcpu )
{
    hthread_t *irq_thread =
	get_hthread_manager()->create_thread( 
		(L4_Word_t)irq_stack[vcpu->cpu_id], sizeof(irq_stack),
		prio, irq_handler_thread, scheduler_tid, pager_tid, vcpu);

    if( !irq_thread )
	return L4_nilthread;

    irq_thread->start();
    
    return irq_thread->get_local_tid();
}

