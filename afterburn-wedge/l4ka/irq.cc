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
 * $Id: irq.cc,v 1.28 2006/01/11 19:01:17 store_mrs Exp $
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
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(user.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(message.h)

#include <device/acpi.h>

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
    bool dispatch_ipc = false, was_dispatch_ipc = false;
    bool deliver_irq = false, do_timer = false;
    word_t reraise_irq = INTLOGIC_INVALID_IRQ, reraise_vector = 0;
    intlogic_t &intlogic = get_intlogic();

    periodic = L4_TimePeriod( timer_length.raw );

    for (;;)
    {
	L4_MsgTag_t tag = L4_ReplyWait_Timeout( ack_tid, periodic, &tid );
	deliver_irq = do_timer = false;
	save_ack_tid = ack_tid;
	ack_tid = L4_nilthread;
	was_dispatch_ipc = dispatch_ipc;
	dispatch_ipc = false;

	if( L4_IpcFailed(tag) )
	{
	    L4_Word_t err = L4_ErrorCode();

	    if( (err & 0xf) == 3 ) { // Receive timeout.
		// Timer interrupt.
		deliver_irq = do_timer = true;
	    }
	    else if( (err & 0xf) == 2 ) { // Send timeout.
		if( was_dispatch_ipc )
		{
		    // User-level programs already in the send queue of the main
		    // thread can beat us to IPC delivery, causing
		    // us to time-out when sending a vector request to the
		    // dispatch loop.
		    if (debug_hwirq || intlogic.is_irq_traced(reraise_irq, reraise_vector)) 
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
			    << " to thread" << save_ack_tid 
			    << " error " << (void *) err
			    << "\n";
			L4_KDB_Enter("BUG");
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
	else switch( L4_Label(tag) )
	{
	    case msg_label_hwirq:
	    {
		ASSERT( tid.global.X.thread_no < tid_user_base );
		ASSERT(CONFIG_DEVICE_PASSTHRU);
		// Hardware IRQ.
		L4_Word_t irq = tid.global.X.thread_no;
		if(debug_hwirq || intlogic.is_irq_traced(irq)) 
		    con << "hardware irq: " << irq
			<< ", int flag: " << get_cpu().interrupts_enabled()
			<< '\n';
		
		/* 
		 * jsXXX: not strictly necessary here, pic/apic will set it also
		 */
		intlogic.set_hwirq_mask(irq);
		intlogic.raise_irq( irq );
		deliver_irq = true;
		break;
	    }
	    case msg_label_hwirq_ack:
	    {
		ASSERT(CONFIG_DEVICE_PASSTHRU);
		L4_Word_t irq;
		msg_hwirq_ack_extract( &irq );
		if(debug_hwirq || intlogic.is_irq_traced(irq))
		    con << "hardware irq ack " << irq << '\n';
		// Send an ack message to the L4 interrupt thread.
		// TODO: the main thread should be able to do this via
		// propagated IPC.
		ack_tid.global.X.thread_no = irq;
		ack_tid.global.X.version = 1;
		L4_LoadMR( 0, 0 );  // Ack msg.
		break;
	    }
	    case msg_label_virq:
	    {
		// Virtual interrupt from external source.
		L4_Word_t irq;
		msg_virq_extract( &irq );
		if( debug_virq )
		    con << "virtual irq: " << irq 
			<< ", from TID " << tid << '\n';
		intlogic.raise_irq( irq );
		break;
	    }		    
#if defined(CONFIG_DEVICE_PASSTHRU)
	    case msg_label_device_enable:
	    {
		L4_Word_t irq;
		L4_Error_t errcode;
		L4_Word_t prio = resourcemon_shared.prio + CONFIG_PRIO_DELTA_IRQ;
		ack_tid = tid;
		msg_device_enable_extract(&irq);
		tid.global.X.thread_no = irq;
		tid.global.X.version = 1;
		errcode = AssociateInterrupt( tid, L4_Myself() );
		    
		if(debug_hwirq || intlogic.is_irq_traced(irq)) 
		    con << "enable device irq: " << irq << '\n';
		    
		if( errcode != L4_ErrOk )
		{
		    con << "Attempt to associate an unavailable interrupt: "
			<< irq << ", L4 error: " 
			<< L4_ErrString(errcode) << ".\n";
		    L4_KDB_Enter("Bla");
		}
		else 
		{
			
		    if( !L4_Set_Priority(tid, prio) )
		    {
			con << "Unable to set irq priority"
			    << " irq " << irq 
			    << " to " << prio
			    << " ErrCode " << L4_ErrorCode() << "\n";
			L4_KDB_Enter("Error");
		    }
		}
		msg_device_done_build();
		break;
	    }
	    case msg_label_device_disable:
	    {
		L4_Word_t irq;
		L4_Error_t errcode;
		ack_tid = tid;
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
		    
		msg_device_done_build();
		break;
	    }
#endif /* defined(CONFIG_DEVICE_PASSTHRU) */
	    default:
		con << "unexpected IRQ message from " << tid << '\n';
		L4_KDB_Enter("BUG");
		break;
	}
	
	if (!deliver_irq)
	    continue;  // Don't attempt other interrupt processing.

	// Make sure that we deliver our timer interrupts too!
	current_time = L4_SystemClock();
	if( do_timer || ((current_time - timer_length) > last_time) )
	{
	    last_time = current_time;

	    if(debug_timer ||  intlogic.is_irq_traced(timer_irq))
		con << ", timer irq " << timer_irq 
		    << ", if: " << get_cpu().interrupts_enabled()
		    << "\n";
	    intlogic.raise_irq( timer_irq );
	}
	
	if( vcpu.in_dispatch_ipc() )
	{
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
	    msg_vector_build( vector );
	    ack_tid = vcpu.main_gtid;
	    if(intlogic.is_irq_traced(irq, vector)) 
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
		prio, vcpu->pcpu_id, irq_handler_thread, scheduler_tid, pager_tid, vcpu);

    if( !irq_thread )
	return L4_nilthread;
    
    vcpu->irq_info.mr_save.load_startup_reply(
	(L4_Word_t) irq_thread->start_ip, (L4_Word_t) irq_thread->start_sp);
    
    return irq_thread->get_local_tid();
}

