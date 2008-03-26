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
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(message.h)

#include <device/acpi.h>
#include <device/i8253.h>
#include <device/rtc.h>

static unsigned char irq_stack[CONFIG_NR_VCPUS][KB(16)] ALIGNED(CONFIG_STACK_ALIGN);
extern i8253_t i8253;
L4_Clock_t timer_length;


static void irq_handler_thread( void *param, hthread_t *hthread )
{
    L4_KernelInterfacePage_t *kip  = (L4_KernelInterfacePage_t *) L4_GetKernelInterface();
    L4_Word_t tid_user_base = L4_ThreadIdUserBase(kip);
    UNUSED L4_Word_t tid_system_base = L4_ThreadIdSystemBase (kip);

    vcpu_t &vcpu = get_vcpu();
    printf( "IRQ thread %t\n", hthread->get_global_tid());

    // Set our thread's exception handler. 
    L4_Set_ExceptionHandler( get_vcpu().monitor_gtid );

    L4_ThreadId_t tid = L4_nilthread;
    L4_ThreadId_t ack_tid = L4_nilthread, save_ack_tid = L4_nilthread;
    L4_Clock_t last_time = {raw: 0}, current_time = {raw: 0}, time_skew = { raw : 0};
    L4_Word_t timer_irq = INTLOGIC_TIMER_IRQ;
    L4_Time_t periodic;
    bool dispatch_ipc = false, was_dispatch_ipc = false;
    bool deliver_irq = false;
    word_t reraise_irq = INTLOGIC_INVALID_IRQ, reraise_vector = 0;
    intlogic_t &intlogic = get_intlogic();
    i8253_counter_t *timer0 = &(i8253.counters[0]);

   
    last_time = L4_SystemClock();

    for (;;)
    {
	timer_length.raw = timer0->get_usecs() ? timer0->get_usecs() : 54925;
	periodic = ( time_skew > timer_length) ? L4_ZeroTime :
	    L4_TimePeriod( timer_length.raw - time_skew.raw);

	L4_MsgTag_t tag = L4_ReplyWait_Timeout( ack_tid, periodic, &tid );
	
	save_ack_tid = ack_tid;
	ack_tid = L4_nilthread;
	was_dispatch_ipc = dispatch_ipc;
	dispatch_ipc = false;
	deliver_irq = false;
	
	current_time = L4_SystemClock();
	rtc.periodic_tick(current_time.raw);

	if( L4_IpcFailed(tag) )
	{
	    L4_Word_t err = L4_ErrorCode();
	    
	    if( (err & 0xf) == 3 ) { // Receive timeout.
		// Timer interrupt.
		deliver_irq = true;
	    }
#if !defined(CONFIG_L4KA_HVM)
	    else if( (err & 0xf) == 2 ) 
	    { 
		// Send timeout.
		if( was_dispatch_ipc )
		{
		    // User-level programs already in the send queue of the main
		    // thread can beat us to IPC delivery, causing
		    // us to time-out when sending a vector request to the
		    // dispatch loop.
		    dprintf(irq_dbg_level(reraise_irq),  
			    "Reraise vector %d irq %d\n", reraise_vector, reraise_irq);
		    intlogic.reraise_vector(reraise_vector);
		}
		else
		{
 		    if (ack_tid.global.X.thread_no >= tid_system_base)
		    {
			printf( "IRQ thread send timeout to TID %t error %d\n", 
				save_ack_tid, err);
			DEBUGGER_ENTER("BUG");
		    }
		}
		continue;
	    }
#endif
	    else 
	    {
		DEBUGGER_ENTER("IRQ IPC failure");
		continue;
	    }
	} /* IPC timeout */
	// Received message.
	else switch( L4_Label(tag) )
	{
	case msg_label_hwirq:
	{
	    ASSERT( tid.global.X.thread_no < tid_user_base );
	    // Hardware IRQ.
	    L4_Word_t irq = tid.global.X.thread_no;
	    dprintf(irq_dbg_level(irq), "hardware irq: %d int flag %d\n", irq, get_cpu().interrupts_enabled());
	    intlogic.raise_irq( irq );
	    deliver_irq = true;
	    break;
	}
	case msg_label_virq:
	{
	    // Virtual interrupt from external source.
	    L4_Word_t irq;
	    msg_virq_extract( &irq );
	    ASSERT(intlogic.is_virtual_hwirq(irq));
	    dprintf(irq_dbg_level(irq), "virtual irq: %d from %t\n", irq, tid);
 	    intlogic.set_virtual_hwirq_sender(irq, tid);
	    intlogic.raise_irq( irq );
	    deliver_irq = true;
	    break;
	}		    
	default:
	    printf( "unexpected IRQ message from %t tag %x\n", tid, tag.raw);
	    DEBUGGER_ENTER("IRQ BUG");
	    break;
	}
	
	if (!deliver_irq)
	    continue;  // Don't attempt other interrupt processing.

	// Make sure that we deliver our timer interrupts too!
	time_skew = time_skew + (current_time - last_time);

	if(time_skew >= timer_length) 
	{
	    u32_t cur = current_time.raw;
	    u32_t last = last_time.raw;
	    u32_t skew = time_skew.raw;
	    u32_t len = timer_length.raw;
	    
	    //cur /= 1000; last /= 1000; skew /= 1000; len /= 1000;
	    
	    dprintf(irq_dbg_level(timer_irq), "timer irq (cur %d last %d skew %d len %d) if %x\n", 
		    cur, last, skew, len, get_cpu().interrupts_enabled());
	    time_skew = time_skew - timer_length;
	    intlogic.raise_irq( timer_irq );
	}
	
	if(time_skew.raw > (1 * timer_length.raw)  ) // ignore skews above timer length
	    time_skew.raw = 0;

	last_time = current_time;
	

	if( vcpu.in_dispatch_ipc() )
	{
	    if (!vcpu.cpu.interrupts_enabled())
		continue;
	    
	    word_t vector, irq;
	    
	    if( !intlogic.pending_vector(vector, irq) )
	    {
		vcpu.cpu.restore_interrupts(true);
		continue;
	    }
	    
	    // Interrupts are enabled if we are in dispatch IPC.
	    dispatch_ipc = true;
	    reraise_irq = irq;
	    reraise_vector = vector;
	    
	    ack_tid = vcpu.main_gtid;
	    dprintf(irq_dbg_level(irq, vector), "forward IRQ %d vector %d via IPC to idle VM TID %t\n", 
		    irq, vector, ack_tid);
	    
   
	    vcpu.load_dispatch_exit_msg(vector, irq);
	}
	else
	{
	    backend_async_deliver_irq( intlogic );
	}

    } /* while */
}

bool irq_init( L4_Word_t prio, L4_ThreadId_t pager_tid, vcpu_t *vcpu )
{
    timer_length.raw = 54925; /* 18.2 HZ */

    hthread_t *irq_thread =
	get_hthread_manager()->create_thread(
	    vcpu,
	    (L4_Word_t)irq_stack[vcpu->cpu_id], 
	    sizeof(irq_stack),
	    prio, 
	    irq_handler_thread, 
	    pager_tid, 
	    vcpu);

    if( !irq_thread )
	return false;

    vcpu->irq_info.mr_save.load_startup_reply(
	(L4_Word_t) irq_thread->start_ip, (L4_Word_t) irq_thread->start_sp);

    vcpu->irq_ltid = irq_thread->get_local_tid();
    vcpu->irq_gtid = L4_GlobalId( vcpu->irq_ltid );

    vcpu->irq_info.mr_save.set_propagated_reply(L4_Pager()); 	
    vcpu->irq_info.mr_save.load();
    L4_Reply(vcpu->irq_gtid);
    

    return true;

}

