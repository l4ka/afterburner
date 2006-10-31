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
#include <l4/thread.h>

#include INC_ARCH(intlogic.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(vm.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(message.h)

#include <device/acpi.h>

static const bool debug_hwirq=0;
static const bool debug_timer=0;
static const bool debug_virq=0;
static const bool debug_ipi=0;
static const bool debug_preemption=0;

static unsigned char irq_stack[CONFIG_NR_VCPUS][KB(16)] ALIGNED(CONFIG_STACK_ALIGN);
static const L4_Clock_t timer_length = {raw: 10000};

cpu_lock_t irq_lock;


static void irq_handler_thread( void *param, hthread_t *hthread )
{
    L4_Word_t tid_user_base = L4_ThreadIdUserBase(L4_GetKernelInterface());
    vcpu_t &vcpu = get_vcpu();
    
    intlogic_t &intlogic = get_intlogic();
#if defined(CONFIG_DEVICE_LAPIC)
    local_apic_t &lapic = get_lapic();
#endif

    L4_ThreadId_t tid = L4_nilthread;
    L4_ThreadId_t ack_tid = L4_nilthread;
    L4_Error_t errcode;
    
    L4_Word_t irq, vector;
    
    con << "IRQ thread, "
	<< "TID " << hthread->get_global_tid() 
	<< "\n";      

    // Set our thread's exception handler. 
    L4_Set_ExceptionHandler( get_vcpu().monitor_gtid );
        
    /*
     * Associate with virtual timing source
     * jsXXX: maybe do that one VAPIC timers are enabled
     */
    tid.global.X.thread_no = INTLOGIC_TIMER_IRQ;
    tid.global.X.version = 1;
    errcode = AssociateInterrupt( tid, L4_Myself() );
    if(debug_timer || intlogic.is_irq_traced(irq)) 
	con << "enable virtual timer irq: " 
	    << INTLOGIC_TIMER_IRQ << '\n';
    if( errcode != L4_ErrOk )
	con << "Unable to associate virtual timer interrupt: "
	    << irq << ", L4 error: " 
	    << L4_ErrString(errcode) << ".\n";
    
    irq_lock.init();
    
    for (;;)
    {
	L4_MsgTag_t tag = L4_ReplyWait( ack_tid, &tid );
	
	if( L4_IpcFailed(tag) )
	{
	    errcode = L4_ErrorCode();
	    con << "VMEXT IRQ failure, thread timeout"
		<< " to thread" << ack_tid  
		<< " from thread" << tid
		<< " error " << (void *) errcode
		<< "\n";
	    DEBUGGER_ENTER();
	    ack_tid = L4_nilthread;
	    continue;
	}
	
	ack_tid = L4_nilthread;
	L4_Set_TimesliceReceiver(L4_nilthread);
	
	// Received message.
	switch( L4_Label(tag) )
	{
	    case msg_label_hwirq:
	    {
		// Hardware IRQ.
		if (tid.global.X.thread_no < tid_user_base )
		{
		    ASSERT(CONFIG_DEVICE_PASSTHRU);
		    irq = tid.global.X.thread_no;
		    if(debug_hwirq || intlogic.is_irq_traced(irq)) 
			con << "hardware irq: " << irq
			    << ", int flag: " << get_cpu().interrupts_enabled()
			    << '\n';
		    
		    /* 
		     * jsXXX: not strictly necessary here, 
		     * pic/apic will set it afterwards as well
		     */
		    intlogic.set_hwirq_mask(irq);

		}
		else
		{
		    /* jsXXX: check vtimer irq source here */
		    irq = INTLOGIC_TIMER_IRQ;
		    if(debug_timer || intlogic.is_irq_traced(irq)) 
		    {
			static L4_Clock_t last_time = {raw: 0};
			L4_Clock_t current_time = L4_SystemClock();
			L4_Word64_t time_diff = (current_time - last_time).raw;
			
			if(debug_timer || intlogic.is_irq_traced(irq)) 
			    con << "vtimer irq: " << irq
				<< " diff " << (L4_Word_t) time_diff
				<< " int flag: " << get_cpu().interrupts_enabled() 
				<< "\n";
			last_time = current_time;
		    }
		}
		
		intlogic.raise_irq( irq );
		/*
		 * If a user level thread was preempted, switch
		 * to the main thread to let him handle the 
		 * preemption IPC
		 */
		if (vcpu.in_dispatch_ipc())
		{
		    if (debug_preemption)
		    {
			con << "forward timeslice to main thread"
			    << "tid " << vcpu.main_gtid
			    << "\n";
			DEBUGGER_ENTER(0);
		    }		    
		    L4_Set_TimesliceReceiver(vcpu.main_gtid);
		}
		else if (vcpu.main_info.mr_save.is_preemption_msg() &&
			!irq_lock.is_locked())
		{
		    ack_tid = vcpu.main_gtid;
		    if (debug_preemption)
			con << "propagate preemption reply to kernel (IRQ)" 
			    << " tid " << ack_tid << "\n";  
		    backend_async_irq_deliver( intlogic ); 
		    vcpu.main_info.mr_save.set_propagated_reply(vcpu.monitor_gtid); 	
		    vcpu.main_info.mr_save.load_preemption_reply();
		} 
		else
		{
		    /*
		     * If main thread was preempted switch to monitor
		     */
		    if (debug_preemption)
		    {
			con << "forward timeslice to monitor thread\n";
		    }		    
		    L4_Set_TimesliceReceiver(vcpu.monitor_gtid);
		}
		break;
	    }
	    case msg_label_hwirq_ack:
	    {
		ASSERT(CONFIG_DEVICE_PASSTHRU);
		msg_hwirq_ack_extract( &irq );
		if(debug_hwirq || intlogic.is_irq_traced(irq))
		    con << "hardware irq ack " << irq << '\n';
		ack_tid.global.X.thread_no = irq;
		ack_tid.global.X.version = 1;
		L4_LoadMR( 0, 0 );  // Ack msg.
		break;
	    }
	    case msg_label_virq:
	    {
		// Virtual interrupt from external source.
		msg_virq_extract( &irq );
		if( debug_virq )
		    con << "virtual irq: " << irq 
			<< ", from TID " << tid << '\n';
		intlogic.raise_irq( irq );
		break;
	    }		    
	    case msg_label_ipi:
	    {
		L4_Word_t src_vcpu_id;		
		msg_ipi_extract( &src_vcpu_id, &vector  );
		if (debug_ipi) 
		    con << " IPI from VCPU " << src_vcpu_id 
			<< " vector " << vector
			<< '\n';
#if defined(CONFIG_DEVICE_LAPIC)
		lapic.raise_vector(vector, INTLOGIC_INVALID_IRQ);;
#endif		
		break;
	    }
#if defined(CONFIG_DEVICE_PASSTHRU)
	    case msg_label_device_enable:
	    {
		msg_device_enable_extract(&irq);
		tid.global.X.thread_no = irq;
		tid.global.X.version = 1;
		    
		if(debug_hwirq || intlogic.is_irq_traced(irq)) 
		    con << "enable device irq: " << irq << '\n';
		
		errcode = AssociateInterrupt( tid, L4_Myself() );

		if( errcode != L4_ErrOk )
		    con << "Attempt to associate an unavailable interrupt: "
			<< irq << ", L4 error: " 
			<< L4_ErrString(errcode) << ".\n";
		break;
	    }
	    case msg_label_device_disable:
	    {
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
		    
		break;
	    }
#endif /* defined(CONFIG_DEVICE_PASSTHRU) */
	    default:
		con << "unexpected IRQ message from " << tid << '\n';
		L4_KDB_Enter("BUG");
		break;
	}


	
    } /* for (;;) */
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

