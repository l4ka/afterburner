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
#include <math.h>
#include <common/basics.h>
#include <common/debug.h>
#include <common/hthread.h>
#include <common/console.h>
#include <resourcemon/vm.h>
#include <resourcemon/virq.h>
#include <resourcemon/resourcemon.h>

#if defined(cfg_eacc)
#include <resourcemon/eacc.h>
#endif

#if defined(cfg_l4ka_vmextensions)
#define VIRQ_BALANCE

L4_ThreadId_t roottask = L4_nilthread;
L4_ThreadId_t s0 = L4_nilthread;
L4_KernelInterfacePage_t * kip = (L4_KernelInterfacePage_t *) 0;

virq_t virqs[IResourcemon_max_cpus];
pirqhandler_t pirqhandler[MAX_IRQS];
L4_Word_t ptimer_irqno_start;

static const L4_Word_t hwirq_timeouts = L4_Timeouts(L4_ZeroTime, L4_Never);
static const L4_Word_t preemption_timeouts = L4_Timeouts(L4_ZeroTime, L4_ZeroTime);

#define MSG_LABEL_PREEMPTION	0xffd0
#define MSG_LABEL_YIELD		0xffd1
#define MSG_LABEL_CONTINUE	0xffd2
#define MSG_LABEL_IRQ		0xfff0
#define MSG_LABEL_IRQ		0xfff0
#define MSG_LABEL_IRQ_ACK	0xfff1

L4_Word_t period_len, num_pcpus;

const L4_MsgTag_t hwirqtag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_IRQ } };
const L4_MsgTag_t continuetag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_CONTINUE} }; 
const L4_MsgTag_t pcontinuetag = (L4_MsgTag_t) { X: { 0, 0, 1, MSG_LABEL_CONTINUE} }; 
const L4_MsgTag_t acktag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_IRQ_ACK} }; ; 
const L4_MsgTag_t packtag = (L4_MsgTag_t) { X: { 0, 0, 1, MSG_LABEL_IRQ_ACK} }; ; 


#undef LATENCY_BENCHMARK
#if defined(LATENCY_BENCHMARK)
#define LD_NR_SAMPLES	   13
#define NR_SAMPLES	   (1 << LD_NR_SAMPLES)
#define NR_SAMPLED_CPUS     1
static void virq_latency_benchmark(virq_t *virq)
{
    if (virq->mycpu < NR_SAMPLED_CPUS) 
    {
	static L4_Word_t preemption_delta[NR_SAMPLED_CPUS][NR_SAMPLES];
	static L4_Word_t irq_delta[NR_SAMPLED_CPUS][NR_SAMPLES];
	static L4_Word_t timer_delta[NR_SAMPLED_CPUS][NR_SAMPLES];
	static L4_Word_t old_preemption_count[NR_SAMPLED_CPUS];
	static u64_typed_t old_time[NR_SAMPLED_CPUS];
	static L4_Word_t idx[NR_SAMPLED_CPUS];
	static bool initialized = false;

	if (!initialized)
	{
	    for (int c=0; c < NR_SAMPLED_CPUS; c++)
	    {
		for (int s=0; s < NR_SAMPLES; s++)
		{
		    preemption_delta[c][s] = 0;
		    irq_delta[c][s] = 0;
		    timer_delta[c][s] = 0;
		}
		old_preemption_count[c] = 0;
		old_time[c].raw = 0;
		idx[c] = 0;
	    }
	    initialized = true;
	}
		    
		    
	L4_Word_t preemption_count = 0;
	u64_typed_t irq_time = { raw : 0}, now = {raw : 0};
	L4_Word_t cpu = virq->mycpu;
		    
	now = ia32_rdtsc();
		    
	L4_StoreMR(1, &irq_time.x[0] );
	L4_StoreMR(2, &irq_time.x[1] );
	L4_StoreMR(3, &preemption_count);
		
	preemption_delta[cpu][idx[cpu]] = preemption_count - old_preemption_count[cpu];
	old_preemption_count[cpu] = preemption_count;
		
	irq_delta[cpu][idx[cpu]] = (L4_Word_t) (now.raw - irq_time.raw);

	timer_delta[cpu][idx[cpu]] = (L4_Word_t) (now.raw - old_time[cpu].raw);
	old_time[cpu].raw = now.raw;
		    
	if (++idx[cpu] == NR_SAMPLES)
	{
	    L4_Word64_t avg_irq_delta = 0, var_irq_delta = 0;
	    L4_Word64_t avg_preemption_delta = 0, var_preemption_delta = 0;
	    L4_Word64_t avg_timer_delta = 0, var_timer_delta = 0;
		    
	    for (idx[cpu]=0; idx[cpu] < NR_SAMPLES; idx[cpu]++)
	    {
		avg_irq_delta += irq_delta[cpu][idx[cpu]];
		avg_preemption_delta += preemption_delta[cpu][idx[cpu]];
		avg_timer_delta += timer_delta[cpu][idx[cpu]];
	    }
	    avg_irq_delta >>= LD_NR_SAMPLES;
	    avg_preemption_delta >>= LD_NR_SAMPLES;
	    avg_timer_delta >>= LD_NR_SAMPLES;
		    
	    for (idx[cpu]=0; idx[cpu] < NR_SAMPLES; idx[cpu]++)
	    {
		var_irq_delta += (avg_irq_delta > irq_delta[cpu][idx[cpu]]) ?
		    ((avg_irq_delta - irq_delta[cpu][idx[cpu]]) * (avg_irq_delta - irq_delta[cpu][idx[cpu]])) : 
		    ((irq_delta[cpu][idx[cpu]] - avg_irq_delta) * (irq_delta[cpu][idx[cpu]] - avg_irq_delta));
		var_preemption_delta += (avg_preemption_delta > preemption_delta[cpu][idx[cpu]]) ?
		    ((avg_preemption_delta - preemption_delta[cpu][idx[cpu]]) * (avg_preemption_delta - preemption_delta[cpu][idx[cpu]])) : 
		    ((preemption_delta[cpu][idx[cpu]] - avg_preemption_delta) * (preemption_delta[cpu][idx[cpu]] - avg_preemption_delta));
		var_timer_delta += (avg_timer_delta > timer_delta[cpu][idx[cpu]]) ?
		    ((avg_timer_delta - timer_delta[cpu][idx[cpu]]) * (avg_timer_delta - timer_delta[cpu][idx[cpu]])) : 
		    ((timer_delta[cpu][idx[cpu]] - avg_timer_delta) * (timer_delta[cpu][idx[cpu]] - avg_timer_delta));
	    }
			
	    var_irq_delta >>= LD_NR_SAMPLES;
	    var_irq_delta = (L4_Word64_t) sqrt((double) var_irq_delta);
			
	    var_preemption_delta >>= LD_NR_SAMPLES;
	    var_preemption_delta = (L4_Word64_t) sqrt((double) var_preemption_delta);

	    var_timer_delta >>= LD_NR_SAMPLES;
	    var_timer_delta = (L4_Word64_t) sqrt((double) var_timer_delta);

	    hout << "VIRQ " << cpu 
		 << " irq latency " << (L4_Word_t) avg_irq_delta
		 << " (var  " << (L4_Word_t) var_irq_delta << ")"
		 << " pfreq " << (L4_Word_t) avg_preemption_delta
		 << " (var  " << (L4_Word_t) var_preemption_delta << ")"
		 << " timer " << (L4_Word_t) avg_timer_delta
		 << " (var  " << (L4_Word_t) var_timer_delta << ")"
		 << "\n"; 
	    idx[cpu] = 0;
	}
    }
}
#endif


static inline L4_Word_t tid_to_handler_idx(virq_t *virq, L4_ThreadId_t tid)
{
    for (word_t idx=0; idx < virq->num_handlers; idx++)
	if (virq->handler[idx].vm->get_monitor_tid(virq->handler[idx].vcpu) == tid)
	    return idx;
    return MAX_VIRQ_HANDLERS;
		
}



static inline bool register_hwirq_handler(vm_t *vm, L4_Word_t hwirq, L4_ThreadId_t handler_tid)
{

   
    ASSERT(vm->vcpu_count);
    L4_Word_t vcpu = 0;
    do 
    {
	if (handler_tid == vm->get_monitor_tid(vcpu))
	    break;
    } while (++vcpu < vm->vcpu_count);
	
    if (vcpu == vm->vcpu_count)
    {
	hout << "VIRQ handler"
	     << " TID " << handler_tid
	     << " not yet registered for vtimer"
	     << "\n";
	return false;
    }
	
    virq_t *virq = &virqs[vm->get_pcpu(vcpu)];
    L4_Word_t pcpu = virq->mycpu;
	
    if (pirqhandler[hwirq].virq != NULL)
    {	
	ASSERT(pirqhandler[hwirq].idx != MAX_VIRQ_HANDLERS);
	vm_t *pirq_vm = virq->handler[pirqhandler[hwirq].idx].vm;
	    
	if (debug_virq)
	    hout << "VIRQ " << hwirq
		 << " already registerd"
		 << " to handler " << pirq_vm->get_monitor_tid(virq->handler[pirqhandler[hwirq].idx].vcpu)
		 << " override with handler " << handler_tid
		 << "\n";
    }
    else 
    {
	    
	L4_ThreadId_t irq_tid;
	irq_tid.global.X.thread_no = hwirq;
	irq_tid.global.X.version = 1;
	
	int result = L4_AssociateInterrupt( irq_tid, virq->myself);
	if( !result )
	{
	    hout << "VIRQ failed to associate IRQ " << hwirq
		 << " to " << handler_tid
		 << "\n";
	    return false;
	}
	    
	L4_Word_t prio = PRIO_IRQ;
	L4_Word_t dummy;
	    
	if ((prio != 255 || pcpu != L4_ProcessorNo()) &&
	    !L4_Schedule(irq_tid, ~0UL, pcpu, prio, ~0UL, &dummy))
	{
		hout << "VIRQ failed to set IRQ handler's " << hwirq
		     << "scheduling parameters\n";
		return false;
	}
	    
    }
    if (debug_virq)
	hout << "VIRQ associate HWIRQ " << hwirq 
	     << " with handler " << handler_tid
	     << " count " << vm->vcpu_count
	     << " vcpu " << vcpu
	     << " pcpu " << pcpu
	     << "\n";

	
    L4_Word_t handler_idx = tid_to_handler_idx(virq, handler_tid);
    ASSERT(handler_idx < MAX_VIRQ_HANDLERS);
	
    pirqhandler[hwirq].virq = virq;
    pirqhandler[hwirq].idx = handler_idx;

    return true;
}

static inline virq_handler_t *register_timer_handler(vm_t *vm, word_t vcpu, word_t pcpu, 
						     L4_ThreadId_t handler_tid, bool migration)
{
    
    ASSERT(pcpu < IResourcemon_max_cpus);
    ASSERT(vcpu < IResourcemon_max_vcpus);
    
    virq_t *virq = &virqs[pcpu];
    ASSERT(virq->mycpu == pcpu);
    
    if (virq->num_handlers == MAX_VIRQ_HANDLERS)
    {
	hout << "VIRQ reached maximum number of handlers"
	     << " (" << virq->num_handlers << ")"
	     << "\n"; 
	return NULL;
    }

    if (vm->get_monitor_tid(vcpu) != L4_nilthread)
    {
	if (debug_virq)
	    hout << "VIRQ monitor tid " << handler_tid 
		 << " vcpu " << vcpu
		 << " already registered for pcpu " << vm->get_pcpu(vcpu)
		 << "\n";
	return NULL;
    }
    
    virq_handler_t *handler = &virq->handler[virq->num_handlers];
    vm->set_virq_tid(virq->mycpu, virq->thread->get_global_tid());
    vm->set_monitor_tid(vcpu, handler_tid);
    vm->set_pcpu(vcpu, virq->mycpu);

    handler->vm = vm;
    handler->vcpu = vcpu;
    virq->num_handlers++;
	
    L4_Word_t dummy;
    if (!L4_Schedule(handler_tid, virq->myself.raw, (1 << 16 | virq->mycpu), ~0UL, L4_PREEMPTION_CONTROL_MSG, &dummy))
    {
	L4_Word_t errcode = L4_ErrorCode();
	if (!migration || L4_ErrorCode() != 5)
	{
	    hout << "VIRQ failed to set scheduling parameters"
		 << " of IRQ handler " << handler_tid
		 << " errcode " << errcode
		 << "\n";
	    return NULL;
	}
	else
	{
	    if (debug_virq)
		hout << "VIRQ couldn't migrate "	      
		     << "IRQ handler " << handler_tid
		     << ", migration still in progress\n";

	    return handler;

	}
    }
	
    if (debug_virq)
	hout << "VIRQ associate TIMER IRQ " << ptimer_irqno_start + virq->mycpu
	     << " virq_tid " <<  virq->thread->get_global_tid()
	     << " with handler " << handler_tid
	     << " virqno " << ptimer_irqno_start + vcpu
	     << " pcpu " << virq->mycpu
	     << "\n";

    return handler;
}


static inline bool unregister_timer_handler(L4_Word_t handler_idx)
{
    virq_t *virq = &virqs[L4_ProcessorNo()];

    vm_t *vm = virq->handler[handler_idx].vm;
    L4_Word_t vcpu = virq->handler[handler_idx].vcpu;
    
    ASSERT(vm);
    ASSERT(vcpu < IResourcemon_max_vcpus);
    
    if (debug_virq)
	hout << "VIRQ deassociate TIMER IRQ " << ptimer_irqno_start + vcpu
	     << " with handler " << vm->get_monitor_tid(vcpu)
	     << " virq_tid " <<  virq->thread->get_global_tid()
	     << " pcpu " << virq->mycpu
	     << "\n"; 

    vm->set_monitor_tid(vcpu, L4_nilthread);
    vm->set_pcpu(vcpu, IResourcemon_max_cpus);

    /* Compact list */
    for (L4_Word_t idx = handler_idx; idx < virq->num_handlers-1; idx++)
    {
	virq->handler[idx].vm = virq->handler[idx+1].vm;
	virq->handler[idx].vcpu = virq->handler[idx+1].vcpu;
	virq->handler[idx].state = virq->handler[idx+1].state;
	virq->handler[idx].period_len = virq->handler[idx+1].period_len;
	virq->handler[idx].last_tick = virq->handler[idx+1].last_tick;
	virq->handler[idx].last_balance = virq->handler[idx+1].last_balance;
	virq->handler[idx].old_pcpu = virq->handler[idx+1].old_pcpu;
	virq->handler[idx].balance_pending = virq->handler[idx+1].balance_pending;
    }	

    virq->handler[virq->num_handlers].vm = NULL;
    virq->handler[virq->num_handlers].vcpu = IResourcemon_max_vcpus;
    virq->handler[virq->num_handlers].period_len = 0;
    virq->handler[virq->num_handlers].last_tick = 0;
    virq->handler[virq->num_handlers].last_balance = 0;
    virq->handler[virq->num_handlers].old_pcpu = IResourcemon_max_cpus;
    virq->handler[virq->num_handlers].balance_pending = false;
    virq->num_handlers--;


    return true;
}


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
	hout << "VIRQ error associating timer irq TID: " << ptimer << "\n"; 
	L4_KDB_Enter("VIRQ BUG");
    }
    L4_Word_t dummy;
    if (!L4_Schedule(ptimer, ~0UL, virq->mycpu, PRIO_IRQ, ~0UL, &dummy))
    {
	hout << "VIRQ error setting timer irq's scheduling parameters TID: " << ptimer << "\n"; 
	L4_KDB_Enter("VIRQ BUG");
	    
    }
    
}
static inline bool migrate_vm(virq_handler_t *handler, virq_t *virq)
{		
#if defined(VIRQ_BALANCE)
#define BALANCE_INTERVAL	(1000)
    return (handler &&
	    handler->balance_pending == false &&		
	    virq->ticks - handler->last_balance > BALANCE_INTERVAL &&
	    handler->last_tick > 20000);
#else
    return false;
#endif
}
static inline void migrate_vcpu(virq_t *virq, L4_Word_t dest_pcpu)
{
    		
    vm_t *vm = virq->current->vm;
    L4_Word_t vcpu = virq->current->vcpu;
    L4_ThreadId_t tid = vm->get_monitor_tid(vcpu);

    static word_t debug_count = 0;
    
    if (0 && ++debug_count % 10 == 0)
	hout << "*";
    
    if (1 || debug_virq)
	hout << "VIRQ " << virq->mycpu << " migrate " 
	     << " tid " << tid 
	     << " last_balance " << virq->current->last_balance
	     << " to " << dest_pcpu
	     << " tick " << virq->ticks
	     << " num_pcpus " << num_pcpus
	     << "\n";; 
		
    unregister_timer_handler(virq->current_idx);	
    virq_handler_t *handler = register_timer_handler(vm, vcpu, dest_pcpu, tid, true); 
    ASSERT(handler);
    handler->balance_pending = true;    
    handler->old_pcpu = virq->mycpu;
    handler->last_balance = virq->ticks;


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
    L4_Word_t hwirq = 0;
    
    ASSERT(virq->myself == L4_Myself());
    ASSERT(virq->mycpu == L4_ProcessorNo());
    if (virq->mycpu == 0)
	init_root_servers(virq);

#if defined(cfg_eacc)
    // initialize performance counters
    eacc.pmc_setup();
#endif

    ptimer.global.X.thread_no = ptimer_irqno_start + virq->mycpu;
    ptimer.global.X.version = 1;
    
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
#if defined(cfg_eacc)
		eacc.write(virq->mycpu);
#endif
#if defined(LATENCY_BENCHMARK)
		virq_latency_benchmark(virq);
#endif

		virq->ticks++;
		L4_Set_MsgTag(acktag);
		tag = L4_Reply(ptimer);
		ASSERT(!L4_IpcFailed(tag));		
		if (virq->current->state == vm_state_preempted)
		{
		    if (debug_virq == 2)
			hout << "VIRQ " << virq->mycpu 
			     << " timer IRQ " << hwirq 
			     << "\n"; 
		    reschedule = true;
		}
		else
		{
		    /*
		     * VM will send a preemption message, receive it.
		     */ 
		    to = L4_nilthread;
		    timeouts = preemption_timeouts;
		}		    
		do_timer = true;
	    }
	    else if (from.raw == 0x1d1e1d1e)
	    {
		if (debug_virq == 2)
 		    hout << "VIRQ " << virq->mycpu << " idle\n";
		__asm__ __volatile__ ("hlt");
		to = L4_nilthread;
		reschedule = false;
		
	    }
	    else
	    {
		hwirq = from.global.X.thread_no;
		ASSERT(hwirq < ptimer_irqno_start);
		ASSERT(pirqhandler[hwirq].virq == virq);
		if (pirqhandler[hwirq].idx == MAX_VIRQ_HANDLERS)
		    break;
		
		virq_handler_t *handler = &virq->handler[pirqhandler[hwirq].idx];

		if (debug_virq)
		{
		    hout << "VIRQ " << virq->mycpu 
			 << " IRQ " << hwirq 
			 << " vcpu " << handler->vm->get_monitor_tid(handler->vcpu)
			 << "\n"; 
		}
		
		virq->handler[pirqhandler[hwirq].idx].vm->set_virq_pending(hwirq);
		if (virq->current->state == vm_state_preempted)
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

	    /* Verify that sender belongs to associated VM */
	    if (pirqhandler[hwirq].virq != virq)
	    {
		if (debug_virq)
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
	    {
		if (debug_virq)
		    hout << "VIRQ " << virq->mycpu 
			 << " IRQ ack " << hwirq 
			 << " by " << from
			 << " (" << to << ")"
			 << "\n"; 
		L4_Set_MsgTag(acktag);
	    }

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
	    else
	    {
		if (do_hwirq || do_timer)
		    reschedule = true;	
		else if (to == roottask)
		{
		    virq->current->state = vm_state_preempted;
		    to = L4_nilthread;
		}
		else 
		    to = from;
	    }
	    L4_Set_MsgTag(continuetag);

	}
	break;
	case MSG_LABEL_YIELD:
	{
	    if (from != virq->current->vm->get_monitor_tid(virq->current->vcpu))
		hout << "VIRQ " << virq->mycpu 
		     << " unexpected yield IPC from " << from
		     << " current handler " << virq->current->vm->get_monitor_tid(virq->current->vcpu)
		     << " tag " << (void *) tag.raw
		     << "\n"; 
	    /* yield,  fetch dest */
	    L4_ThreadId_t dest;
	    L4_StoreMR(13, &dest.raw);
	    to = L4_nilthread;
    
	    if (dest == L4_nilthread || dest == from)
	    {
		/* donate time to first idle thread */
		virq->current->state = vm_state_preempted;	
		for (word_t idx=0; idx < virq->num_handlers; idx++)
		    if (virq->handler[idx].state != vm_state_preempted)
		    {
			virq->current_idx = idx;
			virq->current = &virq->handler[idx];
			to = virq->current->vm->get_monitor_tid(virq->current->vcpu);
		    }
	    }
	    else 
	    {
		/*  verify that it's an IRQ thread on our own CPU */
		virq->current->state = vm_state_preempted;	

		L4_Word_t idx = tid_to_handler_idx(virq, dest);
		if (idx < MAX_VIRQ_HANDLERS)
		{
		    virq->current_idx = idx;
		    virq->current = &virq->handler[idx];
		    to = virq->current->vm->get_monitor_tid(virq->current->vcpu);
		}
	    }
	    L4_Set_MsgTag(continuetag);
	}
	break;
	default:
	{
	    L4_KDB_Enter("VIRQ BUG");
	    hout << "VIRQ " << virq->mycpu 
		 << " unexpected IPC from " << from
		 << " current handler " << virq->current->vm->get_monitor_tid(virq->current->vcpu)
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

	    if (migrate_vm(virq->current, virq))
	    {
		L4_Word_t dest_pcpu = (virq->mycpu + 1) % num_pcpus;
		migrate_vcpu(virq, dest_pcpu);
		
		if (virq->num_handlers == 0)
		    continue;
	    }
	    for (L4_Word_t i = 0; i < virq->num_handlers; i++)
	    {
		/* Deliver pending virq interrupts */
		if (virq->ticks - virq->handler[i].last_tick >= 
		    virq->handler[i].period_len)
		{
		    virq->handler[i].state = vm_state_running;
		    virq->handler[i].last_tick = virq->ticks;
		    virq->handler[i].vm->set_virq_pending(ptimer_irqno_start + virq->handler[i].vcpu);
		}
	    }
	    
	    for (L4_Word_t i = 0; i < virq->num_handlers; i++)
	    {
		virq->scheduled = (virq->scheduled + 1) % virq->num_handlers;
		if (virq->handler[virq->scheduled].state == vm_state_running)
		    break;
	    }
	    
	    virq->current_idx = virq->scheduled;
	    ASSERT(virq->current_idx < virq->num_handlers);
	    

	}
	
	if (do_hwirq)
	{
	    /* hwirq; immediately schedule handler VM
	     * TODO: bvt scheduling */	
	    
	    virq_t *hwirq_virq = pirqhandler[hwirq].virq;
	    word_t hwirq_idx = pirqhandler[hwirq].idx;
	    
	    do_hwirq = false;
	    if (debug_virq)
		hout << "VIRQ hwirq " <<  hwirq 
		     << " handler " << hwirq_virq->handler[hwirq_idx].vm->get_monitor_tid(virq->current->vcpu)
		     << "\n";

	    if (hwirq_virq == virq)
		virq->current_idx = hwirq_idx;
	}
	
	virq->current = &virq->handler[virq->current_idx];
	to = virq->current->vm->get_monitor_tid(virq->current->vcpu);
	virq->current->state = vm_state_running;
 	ASSERT(to != L4_nilthread);		
	L4_Set_MsgTag(continuetag);
	
	if (!virq->current->activated)
	{
	    if (debug_virq)
		hout << "VIRQ activate VCPU thread " << to << "\n";
	    L4_Msg_t msg;
	    L4_Clear( &msg );
	    L4_Append( &msg, virq->current->vm->binary_entry_vaddr );		// ip
	    L4_Append( &msg, virq->current->vm->binary_stack_vaddr );		// sp
	    L4_Load( &msg );
	    virq->current->activated = true;
	}
	
	if (virq->current->balance_pending)
	{
	    ASSERT(virq->current->old_pcpu < IResourcemon_max_cpus);
	    if (debug_virq)
		hout << "VIRQ propagate to " << to 
		     << "after balance from pcpu " << virq->current->old_pcpu
		     << " tid " << virqs[virq->current->old_pcpu].myself
		     << "\n";
		    
	    L4_Set_VirtualSender(virqs[virq->current->old_pcpu].myself);
	    L4_Set_MsgTag(pcontinuetag);
	    virq->current->old_pcpu = IResourcemon_max_cpus;
	    virq->current->balance_pending = false;
	    virq->current->last_balance = virq->ticks;
	    
	}
	


	timeouts = hwirq_timeouts;
    }

    ASSERT(false);
}


bool associate_virtual_interrupt(vm_t *vm, const L4_ThreadId_t irq_tid, const L4_ThreadId_t handler_tid)
{
    /*
     * We install a virq thread that forwards IRQs and ticks with frequency 
     * 10ms / num_virq_handlers
     *  
     * for hq irqs:
     *	   hwirq is encoded in irq
     *	   pcpu id is determined by pcpu of handler
    * for timer irqs:
     *	   vcpu id (virqno) is encoded in irq no (irq = ptimer_start + virqno)
     *	   pcpu id is encoded in version
     */
    
    L4_Word_t irq = irq_tid.global.X.thread_no;
    
    if (irq < ptimer_irqno_start)
    {
	return register_hwirq_handler(vm, irq, handler_tid);
    }
    else if (irq >= ptimer_irqno_start && irq < ptimer_irqno_start + IResourcemon_max_vcpus)
    {
	L4_Word_t pcpu = irq_tid.global.X.version;
	ASSERT(pcpu < IResourcemon_max_cpus);
	virq_t *virq = &virqs[pcpu];
	ASSERT(virq->mycpu == pcpu);
	L4_Word_t vcpu = irq - ptimer_irqno_start;
	virq_handler_t *handler = register_timer_handler(vm, vcpu, pcpu, handler_tid, false);

	if (!handler)
	    return false;
	
	/* jsXXX: make configurable */
	handler->period_len = period_len;
	handler->last_tick = 0;
	handler->state = vm_state_preempted;
	handler->activated = false;
	return true;
	
    }
    else
    {
	hout << "VIRQ attempt to associate invalid IRQ " << irq 
	     << " with handler " << handler_tid
	     << "\n";
	return false;
    }
}


bool deassociate_virtual_interrupt(vm_t *vm, const L4_ThreadId_t irq_tid, const L4_ThreadId_t caller_tid)
{
    
    hout << "VIRQ unregistering handler unimplemented"
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
    num_pcpus = L4_NumProcessors(kip);
    ptimer_irqno_start = L4_ThreadIdSystemBase(kip) - num_pcpus;
    
    /* PIC timer = 2ms, APIC = 1ms */
    if (ptimer_irqno_start == 16)
	period_len = 5;
    else
	period_len = 10;

    if (debug_virq)
	hout << "VIRQ period len " << period_len << "ms\n";

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


    for (L4_Word_t cpu=0; cpu < min(IResourcemon_max_cpus,L4_NumProcessors(kip)); cpu++)
    {
	virq_t *virq = &virqs[cpu];
	
	for (L4_Word_t h_idx=0; h_idx < MAX_VIRQ_HANDLERS; h_idx++)
	{
	    virq->handler[h_idx].vm = NULL;
	    virq->handler[h_idx].vcpu = IResourcemon_max_vcpus;
	    virq->handler[h_idx].period_len = 0;
	    virq->handler[h_idx].last_tick = 0;
	    virq->handler[h_idx].balance_pending = false;    
	    virq->handler[h_idx].old_pcpu = IResourcemon_max_cpus;
	    virq->handler[h_idx].last_balance = 0;

	}
	virq->ticks = 0;
	virq->num_handlers = 0;
	virq->current = 0;
	virq->scheduled = 0;
	
	for (L4_Word_t irq=0; irq < MAX_IRQS; irq++)
	{
	    pirqhandler[irq].idx = MAX_VIRQ_HANDLERS;
	    pirqhandler[irq].virq = NULL;
	}
	
	virq->thread = get_hthread_manager()->create_thread( 
	    (hthread_idx_e) (hthread_idx_virq + cpu), PRIO_VIRQ, l4_has_smallspaces(), virq_thread);

	
	if( !virq->thread )
	{	
	    hout << "Could not install virq TID: " 
		 << virq->thread->get_global_tid() << '\n';
    
	    return;
	} 
	
	if (cpu == 0)
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
	
	
	if (!L4_Set_ProcessorNo(virq->thread->get_global_tid(), cpu))
	{
	    hout << "Error: unable to set virq thread's cpu to " << cpu
		 << ", L4 error code: " << L4_ErrorCode() << '\n';
	    return;
	}
	
	virq->myself = virq->thread->get_global_tid();
	virq->mycpu = cpu;
	
	L4_ThreadId_t ptimer = L4_nilthread;
	ptimer.global.X.thread_no = ptimer_irqno_start + virq->mycpu;
	ptimer.global.X.version = 1;
	L4_Start( virq->thread->get_global_tid() ); 
	associate_ptimer(ptimer, virq);
	

    }
    

}
    


#endif /* defined(cfg_l4ka_vmextensions) */
