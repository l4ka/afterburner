/*********************************************************************
 *                
 * Copyright (C) 2006-2007,  Karlsruhe University
 *                
 * File path:     virq.cc
 * Description:   Virtual Time Source
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include <l4/thread.h>
#include <l4/schedule.h>
#include <l4/kdebug.h>
#include <l4/arch.h>
#include <common/debug.h>
#include <common/hthread.h>
#include <common/console.h>
#include <resourcemon/vm.h>
#include <resourcemon/virq.h>
#include <resourcemon/resourcemon.h>

#if defined(cfg_l4ka_vmextensions)


L4_ThreadId_t roottask = L4_nilthread;
L4_ThreadId_t s0 = L4_nilthread;
L4_KernelInterfacePage_t * kip = (L4_KernelInterfacePage_t *) 0;

virq_t virqs[IResourcemon_max_cpus];
static L4_Word_t ptimer_irqno_start;

#define MSG_LABEL_PREEMPTION	0xffd0
#define MSG_LABEL_YIELD		0xffd1
#define MSG_LABEL_CONTINUE	0xffd2
#define MSG_LABEL_IRQ		0xfff0
#define MSG_LABEL_IRQ		0xfff0
#define MSG_LABEL_IRQ_ACK	0xfff1

const L4_MsgTag_t hwirqtag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_IRQ } };
const L4_MsgTag_t continuetag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_CONTINUE} }; 
const L4_MsgTag_t acktag = (L4_MsgTag_t) { raw : 0 }; 

static inline void init_root_servers(virq_t *virq)
{
    
    if (!L4_EnablePreemptionMsgs(s0))
    {
	hout << "Error: unable to set SIGMA0's preemption ctrl"
	     << ", L4 error code: " << L4_ErrorCode() << '\n';
	L4_KDB_Enter("VIRQ BUG");
    }
    if (!L4_EnablePreemptionMsgs(roottask))
    {
	hout << "Error: unable to set ROOTTASK's  preemption ctrl"
	     << ", L4 error code: " << L4_ErrorCode() << '\n';
	L4_KDB_Enter("VIRQ BUG");
    }
	 
    if (!L4_ThreadControl (s0, s0, virq->myself, s0, (void *) (-1)))
    {
	hout << "Error: unable to set SIGMA0's  scheduler "
	     << " to " << virq->thread->get_global_tid()
	     << ", L4 error code: " << L4_ErrorCode() << '\n';    
	L4_KDB_Enter("VIRQ BUG");
    }
    if (!L4_ThreadControl (roottask, roottask, virq->myself, s0, (void *) -1))
    {
	hout << "Error: unable to set ROOTTASK's  scheduler "
	     << " to " << virq->thread->get_global_tid()
	     << ", L4 error code: " << L4_ErrorCode() << '\n';
	L4_KDB_Enter("VIRQ BUG");
	
    }
}


static inline L4_Word_t tid_to_handler_idx(virq_t *virq, L4_ThreadId_t tid)
{
    for (word_t idx=0; idx < virq->num_handlers; idx++)
	if (virq->handler[idx].tid == tid)
	    return idx;
    return MAX_VIRQ_HANDLERS;
		
}

static void virq_thread( 
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
    
    virq_t *virq = &virqs[L4_ProcessorNo()];
    L4_ThreadId_t to = L4_nilthread;
    L4_Word_t timeouts = L4_Timeouts(L4_ZeroTime, L4_Never);
    L4_ThreadId_t from;
    L4_ThreadId_t ptimer;
    L4_MsgTag_t tag;
    L4_Word_t dummy;
    L4_Word_t hwirq;
   
    virq->mycpu = L4_ProcessorNo();
    virq->myself = L4_Myself();
    if (virq->mycpu == 0)
	init_root_servers(virq);

    ptimer.global.X.thread_no = ptimer_irqno_start + virq->mycpu;
    ptimer.global.X.version = 1;
    
    if (!L4_AssociateInterrupt(ptimer, virq->myself))
    {
	hout << "Virq error associating timer irq TID: " << ptimer << "\n"; 
	L4_KDB_Enter("VIRQ BUG");
    }
    if (!L4_Schedule(ptimer, ~0UL, ~0UL, PRIO_IRQ, ~0UL, &dummy))
    {
	hout << "Virq error setting timer irq's scheduling parameters TID: " << ptimer << "\n"; 
	L4_KDB_Enter("VIRQ BUG");

    }
    
    if (debug_virq)
	hout << "VIRQ TID: " << virq->myself << "\n"; 
    
    L4_Set_MsgTag(continuetag);
    
    bool do_timer = false, do_hwirq = false;
    bool reschedule = false;
    
    for (;;)
    {
	tag = L4_Ipc( to, L4_anythread, timeouts, &from);
	ASSERT(L4_IpcSucceeded(tag));
	reschedule = false;
	
	switch( L4_Label(tag) )
	{
	case MSG_LABEL_IRQ:
	{
	    if (from == ptimer)
	    {
		virq->ticks++;
		L4_Set_MsgTag(acktag);
		tag = L4_Reply(ptimer);
		ASSERT(!L4_IpcFailed(tag));
		to = L4_nilthread;
		if (virq->handler[virq->current].state == vm_state_idle)
		    reschedule = true;
		do_timer = true;
	    }
	    else 
	    {
		hwirq = from.global.X.thread_no;
		ASSERT( hwirq < ptimer_irqno_start );
		ASSERT( virq->pirqhandler[hwirq] < MAX_VIRQ_HANDLERS);
		if (debug_virq)
		    hout << "VIRQ IRQ " << hwirq 
			 << " handler " << virq->handler[virq->pirqhandler[hwirq]].tid
			 << "\n";
		
		to = L4_nilthread;
		if (virq->handler[virq->current].state == vm_state_idle)
		    reschedule = true;
		do_hwirq = true;
	    }
	}
	break;
	case MSG_LABEL_IRQ_ACK:
	{
	    L4_StoreMR( 1, &hwirq );
    
	    if (debug_virq)
		hout << "VIRQ IRQ ack " << hwirq 
		     << " by " << from
		     << " (" << L4_ActualSender() << ")"
		     << "\n"; 
	    
	    if (virq->handler[virq->pirqhandler[hwirq]].tid != from)
	    {
		/* Verify that sender belongs to associated VM */
		L4_Word_t idx = tid_to_handler_idx(virq, from);
		ASSERT(idx < MAX_VIRQ_HANDLERS);
		ASSERT(virq->handler[virq->pirqhandler[hwirq]].vm == 
		       virq->handler[virq->pirqhandler[idx]].vm);
		hout << "+\n";
	    }
	    ASSERT( hwirq < ptimer_irqno_start );

	    if (L4_IpcPropagated(L4_MsgTag()))
		from  = L4_ActualSender();
	    
	    L4_Set_MsgTag(acktag);
	    to.global.X.thread_no = hwirq;
	    to.global.X.version = 1;
	    tag = L4_Reply(to);
	    ASSERT(!L4_IpcFailed(tag));
	    to = L4_ActualSender();
	}
	break;
	case MSG_LABEL_PREEMPTION:
	{
	    /* Got a preemption messsage */
	    if (from == s0 || from == roottask)
		/* Make root servers high prio */
		to = from;
	    else 
		reschedule = true;	
	}
	break;
	case MSG_LABEL_YIELD:
	{
	    ASSERT(from == virq->handler[virq->current].tid);
	    /* 
	     * yield, so fetch dest
	     */
	    L4_ThreadId_t dest;
	    L4_StoreMR(13, &dest.raw);
	    virq->handler[virq->current].state = vm_state_idle;	
	    to = L4_nilthread;
    
	    if (dest == L4_nilthread || dest == from)
	    {
		/* donate time for first idle thread */
		for (word_t idx=0; idx < virq->num_handlers; idx++)
		    if (virq->handler[idx].state != vm_state_idle)
		    {
			virq->current = idx;
			to = virq->handler[virq->current].tid;
		    }
	    }
	    else 
	    {
		/*  verify that it's an IRQ thread on our own CPU */
		L4_Word_t idx = tid_to_handler_idx(virq, dest);
		if (idx < MAX_VIRQ_HANDLERS)
		{
		    virq->current = idx;
		    to = virq->handler[virq->current].tid;
		}
	    }
	    L4_Set_MsgTag(continuetag);
	}
	break;
	default:
	{
	    hout << "VIRQ unexpected IPC"
		 << " current handler " << virq->handler[virq->current].tid
		 << " tag " << (void *) tag.raw
		 << "\n"; 
	    L4_KDB_Enter("Virq BUG");
	}
	break;
	}

	if (!reschedule)
	    continue;
	
	ASSERT(do_timer || do_hwirq);
	if (do_timer)
	{
	    /* Preemption message from after timer IRQ;
	     * perform RR scheduling */	
	    do_timer = false;
	    if (++virq->scheduled == virq->num_handlers)
		virq->scheduled = 0;
	
	    virq->current = virq->scheduled;
	    /* Deliver pending virq interrupts */
	    if (virq->ticks - virq->handler[virq->current].last_tick >= 
		virq->handler[virq->current].period_len)
	    {
		virq->handler[virq->current].last_tick = virq->ticks;
		virq->handler[virq->current].vm->
		    set_virq_pending(virq->mycpu, 
				     ptimer_irqno_start + virq->handler[virq->current].idx);
	    }
	    
	}
	else if (do_hwirq)
	{
	    /* Preemption message from after hwIRQ;
	     * perform timeslice donation
	     * TODO: bvt scheduling */	
	    if (debug_virq)
		hout << "VIRQ IRQ " << hwirq 
		     << " schedule handler " << virq->handler[virq->pirqhandler[hwirq]].tid
		     << "\n";

	    virq->current = virq->pirqhandler[hwirq];
	    virq->handler[virq->pirqhandler[hwirq]].vm->
		set_virq_pending(virq->mycpu, hwirq);
	    do_hwirq = false;
	    
	}
	virq->handler[virq->current].state = vm_state_running;
	to = virq->handler[virq->current].tid;
	L4_Set_MsgTag(continuetag);


    }

}

bool associate_virtual_interrupt(vm_t *vm, const L4_ThreadId_t irq_tid, const L4_ThreadId_t handler_tid)
{
    /*
     * We install a virq thread that forwards IRQs and ticks with frequency 
     * 10ms / num_virq_handlers
     */
    
    L4_Word_t irq = irq_tid.global.X.thread_no;
    L4_Word_t cpu = irq_tid.global.X.version;
    
    if (debug_virq)
	hout << "VIRQ associate PIRQ " << irq 
	     << " with handler " << handler_tid
	     << " pcpu " << cpu
	     << "\n";

    virq_t *virq = &virqs[cpu];
    
    if (virq->num_handlers == 0)
    {
	if (cpu == 0)
	{
	    if (!L4_Set_Priority(s0, PRIO_ROOTSERVER))
	    {
		hout << "Error: unable to set SIGMA0's "
		     << " prio to " << PRIO_ROOTSERVER
		     << ", L4 error code: " << L4_ErrorCode() << '\n';
		L4_KDB_Enter("VIRQ BUG");
	    }
	    if (!L4_Set_Priority(roottask, PRIO_ROOTSERVER))
	    {
		hout << "Error: unable to set ROOTTASK's"
		     << " prio to" << PRIO_ROOTSERVER
		     << ", L4 error code: " << L4_ErrorCode() << '\n';
		L4_KDB_Enter("VIRQ BUG");
	    }
	 
	}
	virq->thread = get_hthread_manager()->create_thread( 
	    (hthread_idx_e) (hthread_idx_virq + cpu), PRIO_VIRQ,
	    virq_thread);
	
	if( !virq->thread )
	{	
	    hout << "Could not install virq TID: " 
		 << virq->thread->get_global_tid() << '\n';
    
	    return false;
	} 

	if (!L4_Set_ProcessorNo(virq->thread->get_global_tid(), cpu))
	{
	    hout << "Error: unable to set virq thread's cpu to " << cpu
		 << ", L4 error code: " << L4_ErrorCode() << '\n';
	    return false;
	}
	
	virq->thread->start();

    }
    
    if (irq >= ptimer_irqno_start)
    {	
	ASSERT(irq == ptimer_irqno_start + cpu);
	
	L4_Word_t handler_idx = 0;
	for (L4_Word_t h_idx=0; h_idx < virq->num_handlers; h_idx++)
	{
	    if (virq->handler[h_idx].vm == vm)
		handler_idx++;
	    if (virq->handler[h_idx].tid == handler_tid)
	    {
		hout << "Vtime handler"
		     << " TID " << handler_tid
		     << " already registered"
		     << "\n";
		return true;
	    }
	}	
	
	if (virq->num_handlers == MAX_VIRQ_HANDLERS)
	{
	    hout << "Virq reached maximum number of handlers"
		 << " (" << virq->num_handlers << ")"
		 << "\n"; 
	    return false;
	}
    
	virq->handler[virq->num_handlers].vm = vm;
	virq->handler[virq->num_handlers].state = vm_state_running;
	virq->handler[virq->num_handlers].tid = handler_tid;
	virq->handler[virq->num_handlers].idx = handler_idx;
	/* jsXXX: make configurable */
	virq->handler[virq->num_handlers].period_len = 10;
	virq->handler[virq->num_handlers].last_tick = 0;
	virq->num_handlers++;
	
	L4_Word_t dummy, errcode;

	errcode = L4_ThreadControl( handler_tid, handler_tid, virq->thread->get_global_tid(), 
				    L4_nilthread, (void *) -1UL );

	if (!errcode)
	{
	    hout << "Virq error: unable to set handler thread's scheduler "
		 << "L4 error " << errcode 
		 << "\n";
	    L4_KDB_Enter("Virq BUG");
	}

	if (!L4_Schedule(handler_tid, (L4_Never.raw << 16) | L4_Never.raw, 
			 cpu, ~0UL, L4_PREEMPTION_CONTROL_MSG, &dummy))
	{
	    hout << "Virq error: failed to set scheduling parameters for irq thread\n";
	    L4_KDB_Enter("Virq BUG");
	}
	
	if (debug_virq)
	    hout << "VIRQ registered timer handler " <<  handler_tid
		 << " virq_tid " <<  virq->thread->get_global_tid()
		 << " cpu " <<  (L4_Word_t) cpu
		 << "\n"; 

	vm->set_virq_tid(cpu, virq->thread->get_global_tid());
    }
    else
    {
	L4_Word_t handler_idx = tid_to_handler_idx(virq, handler_tid);
	if (handler_idx == MAX_VIRQ_HANDLERS)
	{
	    hout << "VIRQ handler"
		 << " TID " << handler_tid
		 << " not yet registered for vtimer"
		 << "\n";
	    return false;
	}
	
	L4_ThreadId_t real_irq_tid = irq_tid;
	real_irq_tid.global.X.version = 1;
	int result = L4_AssociateInterrupt( real_irq_tid, virq->myself);
	if( !result )
	{
	    hout << "VIRQ failed to associate IRQ " << irq
		 << "to " << handler_tid
		 << "\n";
	    return false;
	}
	
	L4_Word_t prio = PRIO_IRQ;
	L4_Word_t dummy;
	
	if ((prio != 255 || cpu != L4_ProcessorNo()) &&
	    !L4_Schedule(real_irq_tid, ~0UL, cpu, prio, ~0UL, &dummy))
	{
	    hout << "VIRQ failed to set IRQ " << irq
		 << "scheduling parameters\n";
	    return false;
	}
	
	ASSERT(virq[cpu].pirqhandler[irq] == MAX_VIRQ_HANDLERS);
	virq[cpu].pirqhandler[irq] = handler_idx;
	
    }

    return true;
}


bool deassociate_virtual_interrupt(vm_t *vm, const L4_ThreadId_t irq_tid, const L4_ThreadId_t caller_tid)
{
    hout << "Virq unregistering handler unimplemented"
	 << " caller" << caller_tid
	 << "\n"; 
    L4_KDB_Enter("Resourcemon BUG");
    return false;
}

void virq_init()
{
    kip = (L4_KernelInterfacePage_t *) L4_GetKernelInterface ();
    roottask = L4_Myself();
    s0 = L4_GlobalId (kip->ThreadInfo.X.UserBase, 1);
    ptimer_irqno_start = L4_ThreadIdSystemBase(kip) - L4_NumProcessors(kip);
	    
    for (L4_Word_t cpu=0; cpu < IResourcemon_max_cpus; cpu++)
    {
	for (L4_Word_t h_idx=0; h_idx < MAX_VIRQ_HANDLERS; h_idx++)
	{
	    virqs[cpu].handler[h_idx].tid = L4_nilthread;
	    virqs[cpu].handler[h_idx].idx = 0;
	}
	virqs[cpu].ticks = 0;
	virqs[cpu].num_handlers = 0;
	virqs[cpu].current = 0;
	virqs[cpu].scheduled = 0;
	for (L4_Word_t irq=0; irq < MAX_IRQS; irq++)
	    virqs[cpu].pirqhandler[irq] = MAX_VIRQ_HANDLERS;
		
    }
    
}

#endif /* defined(cfg_l4ka_vmextensions) */
