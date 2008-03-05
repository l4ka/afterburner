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

#include <console.h>
#include <l4/ipc.h>
#include <l4/kip.h>
#include <l4/schedule.h>
#include <l4/thread.h>
#include <device/acpi.h>
#include <device/rtc.h>

#include INC_ARCH(intlogic.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(monitor.h)


static const L4_Clock_t timer_length = {raw: 10000};

static L4_Word_t max_hwirqs = 0;

virq_t virq VCPULOCAL("virq");

static inline void check_pending_virqs(intlogic_t &intlogic)
{

    L4_Word_t irq = max_hwirqs-1;
    while (get_vcpulocal(virq).bitmap->find_msb(irq))
    {
	if(get_vcpulocal(virq).bitmap->test_and_clear_atomic(irq))
	{
	    dprintf(irq_dbg_level(irq), "Received IRQ %d\n", irq);
	    intlogic.raise_irq( irq );
	}
    }
    if(get_vcpulocal(virq).bitmap->test_and_clear_atomic(get_vcpulocal(virq).vtimer_irq))
    {
	dprintf(irq_dbg_level(INTLOGIC_TIMER_IRQ), "timer irq %d\n", get_vcpulocal(virq).vtimer_irq);
	intlogic.raise_irq(INTLOGIC_TIMER_IRQ);
    }
    
    rtc.periodic_tick(L4_SystemClock().raw);

}
void monitor_loop( vcpu_t & vcpu, vcpu_t &activator )
{
    intlogic_t &intlogic = get_intlogic();
    L4_ThreadId_t from = L4_nilthread;
    L4_ThreadId_t to = L4_nilthread;
    L4_Error_t errcode;
    L4_Word_t irq, vector;
    L4_Word_t timeouts = default_timeouts;
    L4_MsgTag_t tag;

    // Set our thread's exception handler. 
    L4_Set_ExceptionHandler( L4_Pager());
    
    vcpu.main_info.mr_save.load();
    to = vcpu.main_gtid;
    
    for (;;)
    {
	L4_Accept(L4_UntypedWordsAcceptor);
	tag = L4_Ipc( to, L4_anythread, timeouts, &from);
	if ( L4_IpcFailed(tag) )
	{
	    DEBUGGER_ENTER("VMext monitor BUG");
	    errcode = L4_ErrorCode();
	    printf( "VMEXT monitor failure to %t from %t error %d\n", to, from, errcode);
	    to = L4_nilthread;
	    continue;
	}
	
	timeouts = default_timeouts;
	
	// Received message.
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
	case msg_label_pfault_start ... msg_label_pfault_end:
	{
	    thread_info_t *vcpu_info = backend_handle_pagefault(tag, from);
	    ASSERT(vcpu_info);
	    vcpu_info->mr_save.load();
	    to = from;
	}
	break;
	case msg_label_exception:
	{
	    ASSERT (from == vcpu.main_ltid || from == vcpu.main_gtid);
	    vcpu.main_info.mr_save.store_mrs(tag);
		
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
	    break;
	}
	case msg_label_preemption:
	{
	    if (from == vcpu.main_ltid || from == vcpu.main_gtid)
	    {
		vcpu.main_info.mr_save.store_mrs(tag);

		dprintf(debug_preemption, "main thread sent preemption IPC ip %x\n",
			vcpu.main_info.mr_save.get_preempt_ip());
		    
		check_pending_virqs(intlogic);
		bool cxfer = backend_async_irq_deliver(get_intlogic());
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
		vcpu.hthread_info.mr_save.store_mrs(tag);
		    
		dprintf(debug_preemption, "vcpu thread sent preemption IPC %t\n", from);
		    
		/* Did we interrupt main thread ? */
		tag = L4_Receive(vcpu.main_gtid, L4_ZeroTime);
		if (L4_IpcSucceeded(tag))
		{
		    vcpu.main_info.mr_save.store_mrs(tag);
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
	    vcpu.main_info.mr_save.store_mrs(tag);
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
	    msg_virq_extract( &irq );
	    ASSERT(intlogic.is_virtual_hwirq(irq));
	    dprintf(irq_dbg_level(irq), "virtual irq: %d from %t\n", irq, from);
 	    intlogic.set_virtual_hwirq_sender(irq, from);
	    intlogic.raise_irq( irq );
	    
	    printf("virtual irq: %d from %t pm msg %d\n", 
		   irq, from, vcpu.main_info.mr_save.is_preemption_msg());
	    
	    if (!vcpu.main_info.mr_save.is_preemption_msg())
		DEBUGGER_ENTER("XXX");

	    /* fall through */
	}		    
	case msg_label_preemption_reply:
	{	
	    if (L4_Label(tag) == msg_label_preemption_reply)
		dprintf(debug_preemption, "vtimer preemption reply");
		    
	    check_pending_virqs(intlogic);
    
	    if (vcpu.main_info.mr_save.is_preemption_msg())
	    {
		bool cxfer = backend_async_irq_deliver( intlogic );
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
	    L4_Word_t src_vcpu_id;		
	    msg_ipi_extract( &src_vcpu_id, &vector  );
	    dprintf(irq_dbg_level(0, vector), " IPI from VCPU %d vector %d\n", src_vcpu_id, vector);
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
	    from.global.X.version = vcpu.get_pcpu_id();
		    
	    dprintf(irq_dbg_level(irq), "enable device irq: %d\n", irq);

	    errcode = AssociateInterrupt( from, L4_Myself() );
	    if ( errcode != L4_ErrOk )
		printf( "Attempt to associate an unavailable interrupt: %d L4 error: %s",
			irq, L4_ErrString(errcode));
		
	    msg_device_done_build();
	}
	break;
	case msg_label_device_disable:
	{
	    to = from;
	    msg_device_disable_extract(&irq);
	    from.global.X.thread_no = irq;
	    from.global.X.version = vcpu.get_pcpu_id();
		    
	    dprintf(irq_dbg_level(irq), "disable device irq: %d\n", irq);
	    errcode = DeassociateInterrupt( from );
	    if ( errcode != L4_ErrOk )
		printf( "Attempt to  deassociate an unavailable interrupt: %d L4 error: %s",
			irq, L4_ErrString(errcode));
		    
	    msg_device_done_build();
	}
	break;
#endif /* defined(CONFIG_DEVICE_PASSTHRU) */
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

	    break;
	}
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
	default:
	{
	    printf( "unexpected IRQ message from %t tag %x\n", from, tag.raw);
	    DEBUGGER_ENTER("BUG");
	}
	break;
		
	} /* switch(L4_Label(tag)) */
	
    } /* for (;;) */
}

L4_ThreadId_t irq_init( L4_Word_t prio, L4_ThreadId_t pager_tid, vcpu_t *vcpu )
{
    L4_KernelInterfacePage_t *kip  = (L4_KernelInterfacePage_t *) L4_GetKernelInterface();
    IResourcemon_shared_cpu_t * rmon_cpu_shared;

    max_hwirqs = L4_ThreadIdSystemBase(kip) - L4_NumProcessors(kip);
    L4_Word_t pcpu_id = vcpu->get_pcpu_id();
    rmon_cpu_shared = &resourcemon_shared.cpu[pcpu_id];
    
    get_vcpulocal(virq).vtimer_irq = max_hwirqs + vcpu->cpu_id;
    get_vcpulocal(virq).bitmap = (bitmap_t<INTLOGIC_MAX_HWIRQS> *) resourcemon_shared.virq_pending;

    L4_ThreadId_t irq_tid;
    irq_tid.global.X.thread_no = max_hwirqs + vcpu->cpu_id;
    irq_tid.global.X.version = pcpu_id;
    
    dprintf(irq_dbg_level(INTLOGIC_TIMER_IRQ), "associating virtual timer irq %d handler %t\n", 
	    max_hwirqs + vcpu->cpu_id, L4_Myself());
   
    L4_Error_t errcode = AssociateInterrupt( irq_tid, L4_Myself() );
    
    if ( errcode != L4_ErrOk )
	printf( "Unable to associate virtual timer irq %d handler %t L4 error %s\n", 
		max_hwirqs + vcpu->cpu_id, L4_Myself(), L4_ErrString(errcode));

 
    /* Turn of ctrlxfer items */
    L4_Word_t dummy;
    L4_ThreadId_t dummy_tid;
    L4_Msg_t ctrlxfer_msg;
    L4_CtrlXferItem_t conf_items[3];    
    
    conf_items[0] = L4_FaultConfCtrlXferItem(L4_FAULT_PAGEFAULT, 0);
    conf_items[1] = L4_FaultConfCtrlXferItem(L4_FAULT_EXCEPTION, 0);
    conf_items[2] = L4_FaultConfCtrlXferItem(L4_FAULT_PREEMPTION, 0);
    
    L4_Clear (&ctrlxfer_msg);
    L4_Append(&ctrlxfer_msg, (L4_Word_t) 3, conf_items);
    L4_Load (&ctrlxfer_msg);
    L4_ExchangeRegisters (L4_Myself(), L4_EXREGS_CTRLXFER_CONF_FLAG, 0, 0 , 0, 0, L4_nilthread,
			  &dummy, &dummy, &dummy, &dummy, &dummy, &dummy_tid);
    
    dprintf(irq_dbg_level(INTLOGIC_TIMER_IRQ), "virtual timer %d virq tid %t\n", 
	    max_hwirqs + vcpu->cpu_id, vcpu->get_hwirq_tid());


    return vcpu->monitor_ltid;
}

