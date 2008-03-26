/*********************************************************************
 *
 * Copyright (C) 2005,  Unversity of Karlsruhe
 *
 * File path:     afterburn-wedge/l4-common/monitor.cc
 * Description:   The monitor thread, which handles wedge faults
 *                (primarily page faults).
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
#include <l4/schedule.h>
#include <l4/ipc.h>

#include INC_ARCH(page.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(monitor.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(irq.h)

void monitor_loop( vcpu_t & vcpu, vcpu_t &activator )
{
    printf( "Entering monitor loop, TID %t\n", L4_Myself());
    L4_ThreadId_t from = L4_nilthread;
    L4_ThreadId_t to = L4_nilthread;
    L4_Word_t timeouts = default_timeouts;
    thread_info_t *vcpu_info;
    L4_Error_t errcode;
    L4_MsgTag_t tag;

    // Set our thread's exception handler. 
    L4_Set_ExceptionHandler( L4_Pager());

    vcpu.main_info.mr_save.load();
    to = vcpu.main_gtid;
    
    //dbg_irq(5);
    
    for (;;) 
    {
	L4_Accept(L4_UntypedWordsAcceptor);
	tag = L4_Ipc( to, L4_anythread, timeouts, &from);
	timeouts = default_timeouts;
	
	if ( L4_IpcFailed(tag) )
	{
	    DEBUGGER_ENTER("monitor BUG");
	    errcode = L4_ErrorCode();
	    printf( "monitor failure to %t from %t error %d\n", to, from, errcode);
	    to = L4_nilthread;
	    continue;
	}

	switch( L4_Label(tag) )
	{
	case msg_label_migration:
	{
	    printf( "received migration request\n");
	    // reply to resourcemonitor
	    // and get moved over to the new host
	    to = from;
	}
	break;
	case msg_label_thread_create:
	{
	    vcpu_t *tvcpu;
	    L4_Word_t stack_bottom;
	    L4_Word_t stack_size;
	    L4_Word_t prio;
	    hthread_func_t start_func;
	    L4_ThreadId_t pager_tid;
	    void *start_param;
	    void *tlocal_data;
	    L4_Word_t tlocal_size;

	    msg_thread_create_extract((void**) &tvcpu, &stack_bottom, &stack_size, &prio, 
				      (void *) &start_func, &pager_tid, &start_param, &tlocal_data, &tlocal_size); 

	    hthread_t *hthread = get_hthread_manager()->create_thread(tvcpu, stack_bottom, stack_size, prio, 
								      start_func, pager_tid, start_param, 
								      tlocal_data, tlocal_size);
		
	    msg_thread_create_done_build(hthread);
	    to = from;
	}
	break;
	case msg_label_pfault_start ... msg_label_pfault_end:
	{
	    vcpu_info = backend_handle_pagefault(tag, from);
	    ASSERT(vcpu_info);
	    vcpu_info->mr_save.load();
	    to = from;
	}
	break;
	case msg_label_exception:
	{
	    ASSERT (from == vcpu.main_ltid || from == vcpu.main_gtid);
	    vcpu.main_info.mr_save.store(tag);
		
	    if (vcpu.main_info.mr_save.get_exc_number() == IA32_EXC_NOMATH_COPROC)	
	    {
		printf( "FPU main exception, ip %x\n", vcpu.main_info.mr_save.get_exc_ip());
		vcpu.main_info.mr_save.load_exception_reply(true, NULL);
		vcpu.main_info.mr_save.load();
		to = vcpu.main_gtid;
	    }
	    else
	    {
		printf( "Unhandled main exception %d, ip %x no %\n", 
			vcpu.main_info.mr_save.get_exc_number(),
			vcpu.main_info.mr_save.get_exc_ip());
		panic();
	    }
	}
	break;
#if defined(CONFIG_L4KA_VMEXT)
	case msg_label_preemption:
	{
	    if (from == vcpu.main_ltid || from == vcpu.main_gtid)
	    {
		intlogic_t &intlogic = get_intlogic();
		vcpu.main_info.mr_save.store(tag);

		dprintf(debug_preemption, "main thread sent preemption IPC ip %x\n",
			vcpu.main_info.mr_save.get_preempt_ip());
		    
		check_pending_virqs(intlogic);
		bool cxfer = backend_async_deliver_irq(intlogic);
		vcpu.main_info.mr_save.load_preemption_reply(cxfer);
		vcpu.main_info.mr_save.load();
		to = vcpu.main_gtid;
		    
	    }
#if defined(CONFIG_VSMP)
	    else if (vcpu.is_vcpu_hthread(from)  ||
		     (vcpu.is_booting_other_vcpu() && 
		      from == get_vcpu(vcpu.get_booted_cpu_id()).monitor_gtid))
	    {
		to = from;
		vcpu.hthread_info.mr_save.store(tag);
		    
		dprintf(debug_preemption, "vcpu thread sent preemption IPC %t\n", from);
		    
		/* Did we interrupt main thread ? */
		tag = L4_Receive(vcpu.main_gtid, L4_ZeroTime);
		if (L4_IpcSucceeded(tag))
		{
		    vcpu.main_info.mr_save.store(tag);
		    ASSERT(vcpu.main_info.mr_save.is_preemption_msg());
		}
		    
		/* Reply instantly */
		vcpu.hthread_info.mr_save.load_preemption_reply(false);
		vcpu.hthread_info.mr_save.load();
	    }
#endif
	    else
	    {
		L4_Word_t ip;
		L4_StoreMR( OFS_MR_SAVE_EIP, &ip );
		printf( "Unhandled preemption by tid %t\n", from);
		panic();
	    }
		    
	}
	break;
	case msg_label_preemption_yield:
	{
	    ASSERT(from == vcpu.main_ltid || from == vcpu.main_gtid);	
	    vcpu.main_info.mr_save.store(tag);
	    L4_ThreadId_t dest = vcpu.main_info.mr_save.get_preempt_target();
	    L4_ThreadId_t dest_monitor_tid = get_monitor_tid(dest);
		
	    /* Forward yield IPC to the  resourcemon's scheduler */
	    dprintf(debug_preemption, "main thread sent yield IPC dest %t irqdest %t tag %x\n", 
		    dest, dest_monitor_tid, vcpu.main_info.mr_save.get_msg_tag().raw);
		
	    to = vcpu.get_hwirq_tid();
	    vcpu.irq_info.mr_save.load_yield_msg(dest_monitor_tid, false);
	    vcpu.irq_info.mr_save.load();
	    timeouts = vtimer_timeouts;
	}
	break;
	case msg_label_virq:
	{
	    // Virtual interrupt from external source.
	    L4_Word_t irq;
	    intlogic_t &intlogic = get_intlogic();

	    msg_virq_extract( &irq );
	    ASSERT(intlogic.is_virtual_hwirq(irq));
	    dprintf(irq_dbg_level(irq), "virtual irq: %d from %t\n", irq, from);
 	    intlogic.set_virtual_hwirq_sender(irq, from);
	    intlogic.raise_irq( irq );
	    
	    /* fall through */
	}		    
	case msg_label_preemption_reply:
	{	
	    if (L4_Label(tag) == msg_label_preemption_reply)
		dprintf(debug_preemption, "vtimer preemption reply");
		    
	    intlogic_t &intlogic = get_intlogic();
	    check_pending_virqs(intlogic);
    
	    if (vcpu.main_info.mr_save.is_preemption_msg())
	    {
		bool cxfer = backend_async_deliver_irq( intlogic );
		if (!vcpu.is_idle())
		{
		    vcpu.main_info.mr_save.load_preemption_reply(cxfer);
		    vcpu.main_info.mr_save.load();
		    to = vcpu.main_gtid;
		}
		else
		{
		    to = vcpu.get_hwirq_tid();
		    vcpu.irq_info.mr_save.load_yield_msg(L4_nilthread, false);
		    vcpu.irq_info.mr_save.load();
		    timeouts = vtimer_timeouts;
		}
	    }
	    else
	    {
		/* If vcpu isn't preempted yet, we'll receive a preemption
		 * message instantly; reply to nilthread
		 */
		to = L4_nilthread;
	    }

	}
	break;
	case msg_label_ipi:
	{
	    L4_Word_t vector;
	    L4_Word_t src_vcpu_id;		
	    msg_ipi_extract( &src_vcpu_id, &vector  );
	    dprintf(irq_dbg_level(0, vector), " IPI from VCPU %d vector %d\n", src_vcpu_id, vector);
#if defined(CONFIG_VSMP)
	    local_apic_t &lapic = get_lapic();
	    lapic.lock();
	    lapic.raise_vector(vector, INTLOGIC_INVALID_IRQ);
	    lapic.unlock();
#endif		
	    msg_done_build();
	    to = from;
	}
	break;
	case msg_label_hwirq:
	{
	    ASSERT (from.raw == 0x1d1e1d1e);
	    L4_ThreadId_t last_tid;
	    L4_StoreMR( 1, &last_tid.raw );
	    
	    if (vcpu.main_info.mr_save.is_preemption_msg() && !vcpu.is_idle())
	    {
		// We've blocked a hthread and main is preempted, switch to main
		dprintf(debug_preemption, " idle IPC last %t (main preempted) with to %t\n", last_tid, to);
		vcpu.main_info.mr_save.load_preemption_reply(false);
		vcpu.main_info.mr_save.load();
		to = vcpu.main_gtid;
 	    }
	    else
	    {
		dprintf(debug_preemption, "received idle IPC last %t (main blocked) with to %t\n", last_tid, to);
		DEBUGGER_ENTER("IDLE BLOCKED IPC");
		// main is blocked or idle, Just do nothing and idle to VIRQ 
		to = L4_nilthread; 
	    }
	    

	}
	break;
#endif /* defined(CONFIG_L4KA_VMEXT) */	
#if defined(CONFIG_L4KA_HVM)
	case msg_label_hvm_fault_start ... msg_label_hvm_fault_end:
	    ASSERT (from == vcpu.main_ltid || from == vcpu.main_gtid);
	    vcpu.main_info.mr_save.store(tag);
	    
	    dprintf(debug_hvm_fault, "main vfault %x (%d, %c), ip %x\n", 
		    L4_Label(tag), 
		    vcpu.main_info.mr_save.get_hvm_fault_reason(),
		    (vcpu.main_info.mr_save.is_hvm_fault_internal()? 'i' : 'e'),
		    vcpu.main_info.mr_save.get_exc_ip());
	    // process message
	    if( !backend_handle_vfault_message() ) 
	    {
		to = L4_nilthread;
		vcpu.dispatch_ipc_enter();
	    }
	    else
	    {
		dprintf(debug_hvm_fault, "hvm vfault reply %t\n", from);
		to = from;
		vcpu.main_info.mr_save.load();
	    }
	    break;
#endif
	default:
	    printf( "Unhandled message %x from TID %t\n", tag.raw, from);
	    DEBUGGER_ENTER("monitor: unhandled message");


	}
    }
}

