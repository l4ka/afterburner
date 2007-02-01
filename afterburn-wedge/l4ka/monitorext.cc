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
#include <l4/thread.h>

#include INC_ARCH(intlogic.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(user.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(monitor.h)
#include INC_WEDGE(setup.h)

#include <device/acpi.h>

static const bool debug_hwirq=0;
static const bool debug_timer=0;
static const bool debug_virq=1;
static const bool debug_ipi=0;

static const L4_Clock_t timer_length = {raw: 10000};

L4_ThreadId_t vtimer_tid VCPULOCAL("misc") = L4_nilthread;

void monitor_loop( vcpu_t & vcpu, vcpu_t &activator )
{
    L4_Word_t tid_user_base = L4_ThreadIdUserBase(kip);
    intlogic_t &intlogic = get_intlogic();
    L4_ThreadId_t from = L4_nilthread;
    L4_ThreadId_t to = L4_nilthread;
    L4_Word_t pcpu_id = L4_ProcessorNo();
    IResourcemon_shared_cpu_t *rmon_cpu_shared = &resourcemon_shared.cpu[pcpu_id];
    L4_Error_t errcode;
    L4_Word_t irq, vector;
    L4_Word_t timeouts = default_timeouts;
    L4_MsgTag_t tag;
    
    // Set our thread's exception handler. 
    L4_Set_ExceptionHandler( get_vcpu().monitor_gtid );
       
    
    vcpu.main_info.mr_save.load();
    to = vcpu.main_gtid;
    
    for (;;)
    {
	
	tag = L4_Ipc( to, L4_anythread, timeouts, &from);
	if ( L4_IpcFailed(tag) )
	{
	    DEBUGGER_ENTER();
	    errcode = L4_ErrorCode();
	    con << "VMEXT IRQ failure "
		<< " to thread " << to  
		<< " from thread " << from
		<< " error " << (void *) errcode
		<< "\n";
	    to = L4_nilthread;
	    continue;
	}
	
	to = L4_nilthread;
	timeouts = default_timeouts;
	
	// Received message.
	switch( L4_Label(tag) )
	{
	    case msg_label_pfault_start ... msg_label_pfault_end:
	    {
		thread_info_t *vcpu_info = handle_pagefault(tag, from);
		ASSERT(vcpu_info);
		vcpu_info->mr_save.load();
		to = from;
		break;
	    }
	    case msg_label_exception:
	    {
		L4_Word_t ip;
		L4_StoreMR( OFS_MR_SAVE_EIP, &ip );
		con << "Unhandled main exception, ip " << (void *)ip << "\n";
		panic();
	    }
	    case msg_label_preemption:
	    {
 	        ASSERT(from == vcpu.main_gtid);
		if (debug_preemption)
		    con << "main thread sent preemption IPC"
			<< " ip " << (void *) vcpu.main_info.mr_save.get_preempt_ip()
			<< "\n";
		
		vcpu.main_info.mr_save.store_mrs(tag);
		
		if(bit_test_and_clear(vcpu.cpu_id, rmon_cpu_shared->vtimer_irq_pending))
		{
		    if (debug_timer || intlogic.is_irq_traced(irq)) 
			con << "vtimer irq\n";
		    intlogic.raise_irq( INTLOGIC_TIMER_IRQ );
		}
		backend_async_irq_deliver(get_intlogic());
		vcpu.main_info.mr_save.load_preemption_reply();
		vcpu.main_info.mr_save.load();
		to = vcpu.main_gtid;
	    }
	    break;
	    case msg_label_preemption_yield:
	    {
		ASSERT(from == vcpu.main_gtid);	
		vcpu.main_info.mr_save.store_mrs(tag);
		L4_ThreadId_t dest = vcpu.main_info.mr_save.get_preempt_target();
		L4_ThreadId_t dest_monitor_tid = get_monitor_tid(dest);
		
		/* Forward yield IPC to the  resourcemon's scheduler */
		if (debug_preemption)
		{
		    con << "main thread sent yield IPC"
			<< " dest " << dest
			<< " irqdest " << dest_monitor_tid
			<< " tag " << (void *) vcpu.main_info.mr_save.get_msg_tag().raw
			<< "\n";
		}
		to = vtimer_tid;
		vcpu.irq_info.mr_save.load_yield_msg(dest_monitor_tid);
		vcpu.irq_info.mr_save.load();
		timeouts = vtimer_timeouts;
	    }
	    break;
	    case msg_label_preemption_reply:
	    {
		if (debug_preemption)
		    con << "vtimer time donation\n";
		
		ASSERT(from == vtimer_tid);
		if(bit_test_and_clear(vcpu.cpu_id, rmon_cpu_shared->vtimer_irq_pending))
		{
		    if (debug_timer || intlogic.is_irq_traced(irq)) 
			con << "vtimer irq\n";
		    intlogic.raise_irq( INTLOGIC_TIMER_IRQ );
		    
		}
		backend_async_irq_deliver( intlogic );
		ASSERT (vcpu.main_info.mr_save.is_preemption_msg());
		if (!vcpu.is_idle())
		{
		    vcpu.main_info.mr_save.load_yield_reply_msg();
		    vcpu.main_info.mr_save.load();
		    to = vcpu.main_gtid;
		}
		else
		{
		    to = vtimer_tid;
		    vcpu.irq_info.mr_save.load_yield_msg(L4_nilthread);
		    vcpu.irq_info.mr_save.load();
		    timeouts = vtimer_timeouts;
		}
	    }
	    break;
	    case msg_label_hwirq:
	    {
		ASSERT (from.global.X.thread_no < tid_user_base );
		ASSERT(CONFIG_DEVICE_PASSTHRU);
		irq = from.global.X.thread_no;
		
		if (1 || debug_hwirq || intlogic.is_irq_traced(irq)) 
		{
		    con << "hardware irq: " << irq
			<< ", int flag: " << get_cpu().interrupts_enabled()
			<< '\n';
		}
		
		/* 
		 * jsXXX: not strictly necessary here, 
		 * pic/apic will set it afterwards as well
		 */
		intlogic.set_hwirq_mask(irq);
		intlogic.raise_irq( irq );
		
		/* 
		 * Receive preemption IPC from main
		 */
	    }
	    break;
	    case msg_label_hwirq_ack:
	    {
		ASSERT(CONFIG_DEVICE_PASSTHRU);
		msg_hwirq_ack_extract( &irq );
		if (debug_hwirq || intlogic.is_irq_traced(irq))
		    con << "unpropoagated hardware irq ack "
			<< ", irq " << irq 
			<< "\n";
		to.global.X.thread_no = irq;
		to.global.X.version = 1;
		L4_LoadMR( 0, 0 );  // Ack msg.
	    }
	    break;
	    case msg_label_virq:
	    {
		// Virtual interrupt from external source.
		msg_virq_extract( &irq );
		if ( debug_virq )
		    con << "virtual irq: " << irq 
			<< ", from TID " << from << '\n';
		intlogic.raise_irq( irq );
	    }		    
	    break;
	    case msg_label_ipi:
	    {
		L4_Word_t src_vcpu_id;		
		msg_ipi_extract( &src_vcpu_id, &vector  );
		if (debug_ipi) 
		    con << " IPI from VCPU " << src_vcpu_id 
			<< " vector " << vector
			<< '\n';
#if defined(CONFIG_VSMP)
		local_apic_t &lapic = get_lapic();
		lapic.lock();
		lapic.raise_vector(vector, INTLOGIC_INVALID_IRQ);
		lapic.unlock();
#endif		
		msg_ipi_done_build();
		to = from;
	    }
	    break;
#if defined(CONFIG_DEVICE_PASSTHRU)
	    case msg_label_device_enable:
	    {
		to = from;
		msg_device_enable_extract(&irq);
		
		from.global.X.thread_no = irq;
		from.global.X.version = pcpu_id;
		    
		if (debug_hwirq || intlogic.is_irq_traced(irq)) 
		    con << "enable device irq: " << irq << '\n';

		errcode = AssociateInterrupt( from, L4_Myself() );
		

		if ( errcode != L4_ErrOk )
		    con << "Attempt to associate an unavailable interrupt: "
			<< irq << ", L4 error: " 
			<< L4_ErrString(errcode) << ".\n";
		
		msg_device_done_build();
	    }
	    break;
	    case msg_label_device_disable:
	    {
		to = from;
		msg_device_disable_extract(&irq);
		from.global.X.thread_no = irq;
		from.global.X.version = pcpu_id;
		    
		if (debug_hwirq || intlogic.is_irq_traced(irq)) 
		    con << "disable device irq: " << irq
			<< '\n';
		errcode = DeassociateInterrupt( from );
		if ( errcode != L4_ErrOk )
		    con << "Attempt to deassociate an unavailable interrupt: "
			<< irq << ", L4 error: " 
			<< L4_ErrString(errcode) << ".\n";
		    
		msg_device_done_build();
	    }
	    break;
#endif /* defined(CONFIG_DEVICE_PASSTHRU) */
	    default:
	    {
		con << "unexpected IRQ message from " << from 
		    << "tag " << (void *) tag.raw 
		    << "\n";
		L4_KDB_Enter("BUG");
	    }
	    break;
		
	} /* switch(L4_Label(tag)) */
	
    } /* for (;;) */
}

    L4_ThreadId_t irq_init( L4_Word_t prio, 
	    L4_ThreadId_t scheduler_tid, L4_ThreadId_t pager_tid,
	    vcpu_t *vcpu )
    {
    
	/*
	 * Associate with virtual timing source as early as possible
	 * jsXXX: postpone the timing to when VAPIC timer is enabled
	 */
	IResourcemon_shared_cpu_t *rmon_cpu_shared = &resourcemon_shared.cpu[vcpu->pcpu_id];
	L4_ThreadId_t irq_tid;
	irq_tid.global.X.thread_no = INTLOGIC_TIMER_IRQ;
	irq_tid.global.X.version = vcpu->pcpu_id;
	L4_Error_t errcode = AssociateInterrupt( irq_tid, L4_Myself() );
	if ( errcode != L4_ErrOk )
	    con << "Unable to associate virtual timer interrupt: "
		<< INTLOGIC_TIMER_IRQ << ", L4 error: " 
		<< L4_ErrString(errcode) << ".\n";
    
	vtimer_tid = rmon_cpu_shared->vtimer_tid;
	if (debug_timer || get_intlogic().is_irq_traced(INTLOGIC_TIMER_IRQ)) 
	    con << "enable virtual timer"
		<< " irq: " << INTLOGIC_TIMER_IRQ 
		<< " tid: " << vtimer_tid
		<< "\n";

    
	return vcpu->monitor_ltid;
    }

