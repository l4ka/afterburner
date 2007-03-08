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

const bool virq_small = 0;

L4_ThreadId_t roottask = L4_nilthread;
L4_ThreadId_t s0 = L4_nilthread;
L4_KernelInterfacePage_t * kip = (L4_KernelInterfacePage_t *) 0;

virq_t virqs[IResourcemon_max_cpus];
pirqhandler_t pirqhandler[MAX_IRQS];
static L4_Word_t ptimer_irqno_start, ptimer_irqno_end;

static const L4_Word_t hwirq_timeouts = L4_Timeouts(L4_ZeroTime, L4_Never);
static const L4_Word_t preemption_timeouts = L4_Timeouts(L4_ZeroTime, L4_ZeroTime);

#define MSG_LABEL_PREEMPTION	0xffd0
#define MSG_LABEL_YIELD		0xffd1
#define MSG_LABEL_CONTINUE	0xffd2
#define MSG_LABEL_IRQ		0xfff0
#define MSG_LABEL_IRQ		0xfff0
#define MSG_LABEL_IRQ_ACK	0xfff1

const L4_MsgTag_t hwirqtag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_IRQ } };
const L4_MsgTag_t continuetag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_CONTINUE} }; 
const L4_MsgTag_t acktag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_IRQ_ACK} }; ; 
const L4_MsgTag_t packtag = (L4_MsgTag_t) { X: { 0, 0, 1, MSG_LABEL_IRQ_ACK} }; ; 

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
	 
}

static inline void associate_ptimer(L4_ThreadId_t ptimer, virq_t *virq)
{
    if (!L4_AssociateInterrupt(ptimer, virq->myself))
    {
	hout << "Virq error associating timer irq TID: " << ptimer << "\n"; 
	L4_KDB_Enter("VIRQ BUG");
    }
    L4_Word_t dummy;
    if (!L4_Schedule(ptimer, ~0UL, ~0UL, PRIO_IRQ, ~0UL, &dummy))
    {
	hout << "Virq error setting timer irq's scheduling parameters TID: " << ptimer << "\n"; 
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
    L4_ThreadId_t ptimer = L4_nilthread;
    L4_ThreadId_t pirq = L4_nilthread;
    L4_Word_t timeouts = hwirq_timeouts;
    L4_ThreadId_t from;
    L4_MsgTag_t tag;
    L4_Word_t hwirq;
   
    ASSERT(virq->myself == L4_Myself());
    ASSERT(virq->mycpu == L4_ProcessorNo());
    if (virq->mycpu == 0)
	init_root_servers(virq);

    ptimer.global.X.thread_no = ptimer_irqno_start + virq->mycpu;
    ptimer.global.X.version = 1;
    if (!virq_small)
	associate_ptimer(ptimer, virq);
    if (debug_virq)
	hout << "VIRQ TID: " << virq->myself 
	     << " pCPU " << virq->mycpu
	     << "\n"; 
    
    L4_Set_MsgTag(continuetag);
    
    bool do_timer = false, do_hwirq = false;
    bool reschedule = false;
    
    for (;;)
    {
	tag = L4_Ipc( to, L4_anythread, timeouts, &from);
	reschedule = false;
	timeouts = hwirq_timeouts;
	
	if (L4_IpcFailed(tag))
	{
	    		
	    /* 
	     * We get a receive timeout, when the current thread hasn't send a
	     * preemption reply
	     */
	    if ((L4_ErrorCode() & 0xf) == 3 && (do_timer || do_hwirq))
		reschedule = true;
	    else
	    {
		to = L4_nilthread;
		timeouts = hwirq_timeouts;
		continue;
	    }		    
	}
	else switch( L4_Label(tag) )
	{
	case MSG_LABEL_IRQ:
	{
	    if (from == ptimer)
	    {
		virq->ticks++;
		L4_Set_MsgTag(acktag);
		tag = L4_Reply(ptimer);
		ASSERT(!L4_IpcFailed(tag));		
		if (virq->handler[virq->current].state == vm_state_idle)
		    reschedule = true;
		else
		{
		    to = L4_nilthread;
		    timeouts = preemption_timeouts;
		}		    
		do_timer = true;
	    }
	    else 
	    {
		hwirq = from.global.X.thread_no;
		ASSERT(hwirq < ptimer_irqno_start);
		ASSERT(pirqhandler[hwirq].virq == virq);
		
		if (debug_virq)
		    hout << "VIRQ " << virq->mycpu 
			 << " IRQ " << hwirq 
			 << " handler " << virq->handler[pirqhandler[hwirq].idx].tid 
			 << "\n"; 
		
		virq->handler[pirqhandler[hwirq].idx].vm->set_virq_pending(virq->mycpu, hwirq);
		if (virq->handler[virq->current].state == vm_state_idle)
		    reschedule = true;
		else
		{
		    to = L4_nilthread;
		    timeouts = preemption_timeouts;
		}		    
		do_hwirq = true;

	    }
	}
	break;
	case MSG_LABEL_IRQ_ACK:
	{
	    L4_StoreMR( 1, &hwirq );
	    ASSERT( hwirq < ptimer_irqno_start );
    
	    to = (L4_IpcPropagated(L4_MsgTag())) ? L4_ActualSender() : from;
	    
	    if (debug_virq)
		hout << "VIRQ " << virq->mycpu 
		     << " IRQ ack " << hwirq 
		     << " by " << from
		     << " (" << to << ")"
		     << "\n"; 

	    /* Verify that sender belongs to associated VM */
	    if (pirqhandler[hwirq].virq != virq)
	    {
		hout << "VIRQ " << virq->mycpu 
		     << " IRQ remote ack " << hwirq 
		     << " by " << from
		     << " (" << to << ")"
		     << " pirq " << pirqhandler[hwirq].virq->myself
		     << "\n"; 
		L4_Word_t idx = tid_to_handler_idx(virq, from);
		ASSERT(idx < MAX_VIRQ_HANDLERS);
		ASSERT(pirqhandler[hwirq].virq->handler[pirqhandler[hwirq].idx].vm == 
		       virq->handler[idx].vm);
		L4_Set_VirtualSender(pirqhandler[hwirq].virq->myself);
		L4_Set_MsgTag(packtag);

	    }
	    else
		L4_Set_MsgTag(acktag);

	    pirq.global.X.thread_no = hwirq;
	    pirq.global.X.version = 1;
	    tag = L4_Reply(pirq);
	    ASSERT(!L4_IpcFailed(tag));
	    L4_Set_MsgTag(acktag);
	    
	}
	break;
	case MSG_LABEL_PREEMPTION:
	{
	    /* Got a preemption messsage */
	    if (from == s0 || from == roottask)
	    {
		/* Make root servers high prio */
		to = from;
	    }
	    else if (do_hwirq || do_timer)
		reschedule = true;	
	    else /* if (to == roottask)*/
		/* hout << "+" << to << "\n" */;
	    L4_Set_MsgTag(continuetag);

	}
	break;
	case MSG_LABEL_YIELD:
	{
	    if (from != virq->handler[virq->current].tid)
		hout << "VIRQ " << virq->mycpu 
		     << " unexpected yield IPC from " << from
		     << " current handler " << virq->handler[virq->current].tid
		     << " tag " << (void *) tag.raw
		     << "\n"; 
		
	    ASSERT(from == virq->handler[virq->current].tid);
	    /* yield,  fetch dest */
	    L4_ThreadId_t dest;
	    L4_StoreMR(13, &dest.raw);
	    to = L4_nilthread;
    
	    if (dest == L4_nilthread || dest == from)
	    {
		/* donate time for first idle thread */
		virq->handler[virq->current].state = vm_state_idle;	
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
		virq->handler[virq->current].state = vm_state_idle;	

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
	    L4_KDB_Enter("Virq BUG");
	    hout << "VIRQ " << virq->mycpu 
		 << " unexpected IPC from " << from
		 << " current handler " << virq->handler[virq->current].tid
		 << " tag " << (void *) tag.raw
		 << "\n"; 
	}
	break;
	}

	if (!reschedule || virq->num_handlers == 0)
	    continue;
	
	if (do_timer)
	{
	    /* Preemption after timer IRQ; perform RR scheduling */	
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
		    set_virq_pending(virq->mycpu, virq->handler[virq->current].virqno);
	    }

	}
	
	if (do_hwirq)
	{
	    do_hwirq = false;
	    /* Preemption  after hwIRQ; immediately schedule dest
	     * TODO: bvt scheduling */	
	    virq->current = pirqhandler[hwirq].idx;
	}
	
	//if (!do_timer && !do_hwirq)
	//{
	/* Preemption after in-kernel event (IPI, etc.)
	 * continue running current thread */
	//L4_KDB_Enter("Preemption after In-kernel event");
	//}
	
	virq->handler[virq->current].state = vm_state_running;
	to = virq->handler[virq->current].tid;
	L4_Set_MsgTag(continuetag);
	timeouts = hwirq_timeouts;
    }

    ASSERT(false);
}

bool associate_virtual_interrupt(vm_t *vm, const L4_ThreadId_t irq_tid, const L4_ThreadId_t handler_tid)
{
    /*
     * We install a virq thread that forwards IRQs and ticks with frequency 
     * 10ms / num_virq_handlers
     */
    
    L4_Word_t irq = irq_tid.global.X.thread_no;
    L4_Word_t pcpu = irq_tid.global.X.version;
    
    ASSERT(pcpu < IResourcemon_max_cpus);
    virq_t *virq = &virqs[pcpu];
    
    if (virq->num_handlers == 0)
    {
	if (pcpu == 0)
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
	    (hthread_idx_e) (hthread_idx_virq + pcpu), PRIO_VIRQ, virq_small, virq_thread);

	
	if( !virq->thread )
	{	
	    hout << "Could not install virq TID: " 
		 << virq->thread->get_global_tid() << '\n';
    
	    return false;
	} 
	
	if (pcpu == 0)
	{
	    if (!L4_ThreadControl (s0, s0, virq->thread->get_global_tid(), s0, (void *) (-1)))
	    {
		hout << "Error: unable to set SIGMA0's  scheduler "
		     << " to " << virq->thread->get_global_tid()
		     << ", L4 error code: " << L4_ErrorCode() << '\n';    
		L4_KDB_Enter("VIRQ BUG");
	    }
	    if (!L4_ThreadControl (roottask, roottask, virq->thread->get_global_tid(), s0, (void *) -1))
	    {
		hout << "Error: unable to set ROOTTASK's  scheduler "
		     << " to " << virq->thread->get_global_tid()
		     << ", L4 error code: " << L4_ErrorCode() << '\n';
		L4_KDB_Enter("VIRQ BUG");
	    }
	}
	
	
	if (!L4_Set_ProcessorNo(virq->thread->get_global_tid(), pcpu))
	{
	    hout << "Error: unable to set virq thread's cpu to " << pcpu
		 << ", L4 error code: " << L4_ErrorCode() << '\n';
	    return false;
	}
	
	virq->myself = virq->thread->get_global_tid();
	virq->mycpu = pcpu;
	
	if (virq_small)
	{
	    L4_ThreadId_t ptimer = L4_nilthread;
	    ptimer.global.X.thread_no = ptimer_irqno_start + virq->mycpu;
	    ptimer.global.X.version = 1;
	    associate_ptimer(ptimer, virq);
	}


	L4_Start( virq->thread->get_global_tid() ); 

    }
    
    if (irq >= ptimer_irqno_start && irq < ptimer_irqno_end)
    {	
	if (virq->num_handlers == MAX_VIRQ_HANDLERS)
	{
	    hout << "Virq reached maximum number of handlers"
		 << " (" << virq->num_handlers << ")"
		 << "\n"; 
	    return false;
	}
	L4_Word_t virqno = ptimer_irqno_start;
	
	for (L4_Word_t cpu=0; cpu < IResourcemon_max_cpus; cpu++)
	    for (L4_Word_t idx=0; idx < virqs[cpu].num_handlers; idx++)
	    {
		if (virqs[cpu].handler[idx].vm == vm)
		    virqno++;
		
		if (virqs[cpu].handler[idx].tid == handler_tid)
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
	virq->handler[virq->num_handlers].virqno = virqno;
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
			 pcpu, ~0UL, L4_PREEMPTION_CONTROL_MSG, &dummy))
	{
	    hout << "Virq error: failed to set scheduling parameters for irq thread\n";
	    L4_KDB_Enter("Virq BUG");
	}
	
	if (debug_virq)
	    hout << "VIRQ associate TIMER IRQ " << irq 
		 << " virq_tid " <<  virq->thread->get_global_tid()
		 << " with handler " << handler_tid
		 << " pcpu " << pcpu
		 << " virqno " << virqno
		 << "\n";

	vm->set_virq_tid(pcpu, virq->thread->get_global_tid());
	return true;
	
    }
    else if (irq < ptimer_irqno_start)
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
		 << " to " << handler_tid
		 << "\n";
	    return false;
	}
	
	L4_Word_t prio = PRIO_IRQ;
	L4_Word_t dummy;
	
	if ((prio != 255 || pcpu != L4_ProcessorNo()) &&
	    !L4_Schedule(real_irq_tid, ~0UL, pcpu, prio, ~0UL, &dummy))
	{
	    hout << "VIRQ failed to set IRQ " << irq
		 << "scheduling parameters\n";
	    return false;
	}
	
	ASSERT(pirqhandler[irq].virq == NULL);
	ASSERT(pirqhandler[irq].idx == MAX_VIRQ_HANDLERS);
	pirqhandler[irq].virq = virq;
	pirqhandler[irq].idx = handler_idx;
	
	if (debug_virq)
	    hout << "VIRQ associate HWIRQ " << irq 
		 << " with handler " << handler_tid
		 << " pcpu " << pcpu
		 << "\n";

	
	return true;
    }
    else
    {
	hout << "VIRQ attempt to associate invalid IRQ " << irq 
	     << " virq_tid " <<  virq->thread->get_global_tid()
	     << " with handler " << handler_tid
	     << " pcpu " << pcpu
	     << "\n";
	return false;
    }
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
    ptimer_irqno_end = L4_ThreadIdSystemBase(kip);
	    
    for (L4_Word_t cpu=0; cpu < IResourcemon_max_cpus; cpu++)
    {
	for (L4_Word_t h_idx=0; h_idx < MAX_VIRQ_HANDLERS; h_idx++)
	{
	    virqs[cpu].handler[h_idx].tid = L4_nilthread;
	    virqs[cpu].handler[h_idx].virqno = 0;
	}
	virqs[cpu].ticks = 0;
	virqs[cpu].num_handlers = 0;
	virqs[cpu].current = 0;
	virqs[cpu].scheduled = 0;
	for (L4_Word_t irq=0; irq < MAX_IRQS; irq++)
	{
	    pirqhandler[irq].idx = MAX_VIRQ_HANDLERS;
	    pirqhandler[irq].virq = NULL;
	}
		
    }
    
}

#endif /* defined(cfg_l4ka_vmextensions) */
