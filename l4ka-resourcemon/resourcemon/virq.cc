/*********************************************************************
 *                
 * Copyright (C) 2006-2008,  Karlsruhe University
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
#include <resourcemon/vm.h>
#include <resourcemon/virq.h>
#include <resourcemon/resourcemon.h>
#include <resourcemon/freq_powernow.h>
#include <resourcemon/freq_scaling.h>

#include <common/ia32/msr.h>

#if defined(cfg_eacc)
#include <resourcemon/eacc.h>
#endif


#if defined(cfg_l4ka_vmextensions)
#undef VIRQ_PFREQ

#undef VIRQ_BALANCE
#define VIRQ_BALANCE_INTERVAL_MS	(10)
#define VIRQ_BALANCE_DEBUG 1

L4_ThreadId_t roottask = L4_nilthread;
L4_ThreadId_t roottask_local = L4_nilthread;
L4_ThreadId_t s0 = L4_nilthread;
L4_Word_t user_base = 0;
L4_KernelInterfacePage_t * kip = (L4_KernelInterfacePage_t *) 0;

virq_t virqs[IResourcemon_max_cpus];
virq_handler_t dummy_handler;

pirqhandler_t pirqhandler[MAX_IRQS];
L4_Word_t ptimer_irqno_start;

#define CURRENT_TID()						\
    (virq->current->activated					\
     ? virq->current->vm->get_monitor_tid(virq->current->vcpu)	\
     : L4_nilthread)
#define CURRENT_STATE()				\
    ((virq->current->activated)			\
     ? virq->current->state :			\
     (L4_Word_t) -1)

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
		    
	now = x86_rdtsc();
		    
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

	    printf( "VIRQ %d irq latency %d (var %d) pfreq %d (var %d) timer %d (var %d)\n",
		    cpu, avg_irq_delta, var_irq_delta, avg_preemption_delta, var_preemption_delta,
		    avg_timer_delta, var_timer_delta);
	    idx[cpu] = 0;
	}
    }
}
#endif


static inline bool is_system_task(L4_ThreadId_t tid)
{
    ASSERT(user_base);
    return (tid != L4_nilthread && 
	    (tid == s0 || tid == roottask_local || tid == roottask || 
	     tid.global.X.thread_no < user_base));
}

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
	printf( "VIRQ handler TID %t not yet registered for vtimer\n", handler_tid);
	return false;
    }
	
    virq_t *virq = &virqs[vm->get_pcpu(vcpu)];
    L4_Word_t pcpu = virq->mycpu;
	
    if (pirqhandler[hwirq].virq != NULL)
    {	
	ASSERT(pirqhandler[hwirq].idx != MAX_VIRQ_HANDLERS);
	vm_t *pirq_vm = virq->handler[pirqhandler[hwirq].idx].vm;
	    
	dprintf(debug_virq-2, "VIRQ %d already registered to handler %t override with handler %t\n", 
		hwirq, pirq_vm->get_monitor_tid(virq->handler[pirqhandler[hwirq].idx].vcpu),
		handler_tid);
    }
    else 
    {
	    
	L4_ThreadId_t irq_tid;
	irq_tid.global.X.thread_no = hwirq;
	irq_tid.global.X.version = 1;
	
	int result = L4_AssociateInterrupt( irq_tid, virq->myself);
	if( !result )
	{
	    printf( "VIRQ failed to associate IRQ %d to %t\n", hwirq, handler_tid);
	    return false;
	}
	    
	L4_Word_t prio = PRIO_IRQ;
	L4_Word_t dummy;
	    
	if ((prio != 255 || pcpu != L4_ProcessorNo()) &&
	    !L4_Schedule(irq_tid, ~0UL, pcpu, prio, ~0UL, &dummy))
	{
		printf( "VIRQ failed to set IRQ handler's ",hwirq
		    ,"scheduling parameters\n");
		return false;
	}
	    
    }
    dprintf(debug_virq, "VIRQ associate HWIRQ %d with handler %t count %d vcpu %d pcpu %d\n",
	    hwirq , handler_tid, vm->vcpu_count, vcpu, pcpu);

	
    L4_Word_t handler_idx = tid_to_handler_idx(virq, handler_tid);
    ASSERT(handler_idx < MAX_VIRQ_HANDLERS);
	
    pirqhandler[hwirq].virq = virq;
    pirqhandler[hwirq].idx = handler_idx;

    return true;
}

static inline virq_handler_t *register_timer_handler(vm_t *vm, word_t vcpu, word_t pcpu, 
						     L4_ThreadId_t handler_tid, 
						     word_t period_len, vm_state_e state,
						     bool activated, bool failable)
{
    
    ASSERT(pcpu < IResourcemon_max_cpus);
    ASSERT(vcpu < IResourcemon_max_vcpus);
    
    virq_t *virq = &virqs[pcpu];
    ASSERT(virq->mycpu == pcpu);

    if (virq->num_handlers == MAX_VIRQ_HANDLERS)
    {
	printf( "VIRQ reached maximum number of handlers (%x)\n", virq->num_handlers);
	return NULL;
    }

    if (vm->get_monitor_tid(vcpu) != L4_nilthread)
    {
	dprintf(debug_virq-2, "VIRQ monitor tid %t vcpu %d already registered for pcpu %d\n", 
		handler_tid , vcpu, vm->get_pcpu(vcpu));
	return NULL;
    }
   
    L4_Word_t dummy;
    if (!L4_Schedule(handler_tid, virq->myself.raw, (1 << 16 | virq->mycpu), ~0UL, ~0, &dummy))
    {
	L4_Word_t errcode = L4_ErrorCode();
	
	dprintf(debug_virq - (failable ? 0 : 3),
		"VIRQ failed to set scheduling parameters of handler %t error %d (migration in progress?)\n",
		handler_tid, errcode);
	
	return NULL;
    }

    virq_handler_t *handler = &virq->handler[virq->num_handlers];
    vm->set_virq_tid(virq->mycpu, virq->thread->get_global_tid());
    vm->set_monitor_tid(vcpu, handler_tid);
    vm->set_pcpu(vcpu, virq->mycpu);
	

    handler->vm = vm;
    handler->vcpu = vcpu;
    handler->state = state;
    handler->period_len = period_len;
    handler->activated = activated;
    
    virq->num_handlers++;
	
    dprintf(debug_virq, "VIRQ associate TIMER IRQ %d virq_tid %t with handler %t virqno %d pcpu %d\n",
	    ptimer_irqno_start + virq->mycpu, virq->thread->get_global_tid(), 
	    handler_tid, ptimer_irqno_start + vcpu, virq->mycpu);
    

    return handler;
}


static inline bool unregister_timer_handler(L4_Word_t handler_idx)
{
    virq_t *virq = &virqs[L4_ProcessorNo()];

    vm_t *vm = virq->handler[handler_idx].vm;
    L4_Word_t vcpu = virq->handler[handler_idx].vcpu;
    
    ASSERT(vm);
    ASSERT(vcpu < IResourcemon_max_vcpus);
    
    dprintf(debug_virq, "VIRQ deassociate TIMER IRQ %d with handler %t virq_tid %t pcpu %d\n",
	    ptimer_irqno_start + vcpu, vm->get_monitor_tid(vcpu), 
	    virq->thread->get_global_tid(), virq->mycpu);

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
    virq->handler[virq->num_handlers].state = vm_state_preempted;

    virq->num_handlers--;

    if (virq->num_handlers == 0)
	virq->current = &dummy_handler;

    return true;
}


static inline void init_root_servers(virq_t *virq)
{
    L4_Word_t dummy;
    if (!L4_Schedule(s0, virq->myself.raw, (1 << 16 | virq->mycpu), ~0UL, ~0, &dummy))
    {
	printf( "Error: unable to set SIGMA0's scheduler, L4 error code: %d\n", L4_ErrorCode());
	L4_KDB_Enter("VIRQ BUG");
    }
    if (!L4_Schedule(roottask, virq->myself.raw, (1 << 16 | virq->mycpu), ~0UL, ~0, &dummy))
    {
	printf( "Error: unable to set ROOTTASK's scheduler, L4 error code: %d\n", L4_ErrorCode());
	L4_KDB_Enter("VIRQ BUG");
    }
	 
}

static inline void associate_ptimer(L4_ThreadId_t ptimer, virq_t *virq)
{
    if (!L4_AssociateInterrupt(ptimer, virq->myself))
    {
	printf( "VIRQ error associating timer irq TID: ",ptimer,"\n"); 
	L4_KDB_Enter("VIRQ BUG");
    }
    L4_Word_t dummy;
    if (!L4_Schedule(ptimer, ~0UL, virq->mycpu, PRIO_IRQ, ~0UL, &dummy))
    {
	printf( "VIRQ error setting timer irq's scheduling parameters TID: ",ptimer,"\n"); 
	L4_KDB_Enter("VIRQ BUG");
	    
    }
    
}
static inline bool migrate_vm(virq_handler_t *handler, virq_t *virq)
{		
#if defined(VIRQ_BALANCE)
    return (handler &&
	    handler->balance_pending == false &&		
	    virq->ticks - handler->last_balance > VIRQ_BALANCE_INTERVAL_MS &&
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
    vm_state_e state  = virq->current->state;
    word_t period_len = virq->current->period_len;
    
    
    dprintf(debug_virq, "VIRQ %d migrate tid %t last_balance %d to %d tick %d num_pcpus %d\n",
	    virq->mycpu, tid, (word_t) virq->current->last_balance, dest_pcpu,
	    (word_t) virq->ticks, num_pcpus);


    
    unregister_timer_handler(virq->current_idx);
    
    L4_MsgTag_t tag = L4_Receive (tid, L4_ZeroTime);
    if (!L4_IpcFailed(tag))
    {
	printf("VIRQ %d received pending preemption before migration from tid %td\n",  
	       virq->mycpu);
	state = vm_state_preempted;
    }

    virq_handler_t *handler = register_timer_handler(vm, vcpu, dest_pcpu, tid, period_len, 
						     state, true, false); 
    ASSERT(handler);
    handler->balance_pending = true;    
    handler->old_pcpu = virq->mycpu;
    handler->last_balance = virqs[dest_pcpu].ticks; // MANUEL: set last_balance to the current "time" of the NEW pcpu
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
    bool idle = false;

    dprintf(debug_virq, "VIRQ %d started\n", virq->mycpu);
    
    for (;;)
    {

	if (to == L4_nilthread && idle == true)
	{
	    dprintf(debug_virq, "VIRQ %d idle %t %t\n", virq->mycpu, to, from);
	    __asm__ __volatile__ ("hlt" ::: "memory" );
	}
	idle = false;

	tag = L4_Ipc( to, L4_anythread, timeouts, &from);
	timeouts = hwirq_timeouts;
	reschedule = false;
	
	if (L4_IpcFailed(tag))
	{
	    if ((L4_ErrorCode() & 0xf) == 3)
	    {
		ASSERT(do_timer || do_hwirq);
		/* 
		 * We get a receive timeout, when the current thread hasn't send a
		 * preemption reply, (e.g., because it's waiting for roottask
		 * service and we didn't get an IDLE IPC)
		 */
		
		dprintf(debug_virq, "VIRQ %d receive timeout to %t from %t current  %t state %d\n", 
			virq->mycpu, to, from, CURRENT_TID(), CURRENT_STATE());
		virq->current->state = vm_state_blocked;
		
		reschedule = true;
	    }
	    else
	    {
		/* 
		 * We get a send timeout, when trying to send to a non-preempted
		 * thread (e.g., if thread is waiting for roottask service or 
		 * polling
		 */
		dprintf(debug_virq, "VIRQ %d IPC timeout to %t from %t current %t state %d\n", 
			virq->mycpu, to, from, CURRENT_TID(), CURRENT_STATE());
		to = L4_nilthread;
		continue;
	    }		    
	}
	else switch( L4_Label(tag) )
	{
	case MSG_LABEL_IRQ:
	{
	    if (from.raw == 0x1d1e1d1e)
	    {
		dprintf(debug_virq + 1, "VIRQ %d idle IPC num VMs %d", 
			virq->mycpu, virq->num_handlers);
		if (virq->num_handlers)
		{
		    if (to == roottask_local || to == roottask)
		    {
			dprintf(debug_virq, "VIRQ %d idle IPC last roottask current %t state %d\n", 
				virq->mycpu, CURRENT_TID(), CURRENT_STATE());
			virq->current->state = vm_state_preempted;
		    }
		    else if (virq->current->state == vm_state_running)
		    {
			dprintf(debug_virq, "VIRQ %d idle IPC, block %t state %d\n", virq->mycpu, 
				CURRENT_TID(), CURRENT_STATE());
			virq->current->state = vm_state_blocked;
		    }
		}
		reschedule = true;
		idle = true;
	    }
	    else
	    {
		if (from == ptimer)
		{
#if 0 && defined(cfg_eacc)
		    eacc.write(virq->mycpu);
#endif
#if defined(LATENCY_BENCHMARK)
		    virq_latency_benchmark(virq);
#endif
		    virq->ticks++;
		    
		    dprintf(debug_virq + 1, "VIRQ %d timer IRQ %d current %t state %d\n", 
			    virq->mycpu, from.global.X.thread_no, CURRENT_TID(), CURRENT_STATE());

		    L4_Set_MsgTag(acktag);
		    tag = L4_Reply(ptimer);
		    ASSERT(!L4_IpcFailed(tag));		
		    do_timer = true;
		}
		else
		{
		    hwirq = from.global.X.thread_no;
		    ASSERT(hwirq < ptimer_irqno_start);
		    ASSERT(pirqhandler[hwirq].virq == virq);

		    if (pirqhandler[hwirq].idx == MAX_VIRQ_HANDLERS)
			break;
		
		    virq_handler_t *handler = &virq->handler[pirqhandler[hwirq].idx];

		    dprintf(debug_virq, "VIRQ %d IRQ %d handler %t\n",
			    virq->mycpu, hwirq, handler->vm->get_monitor_tid(handler->vcpu));
		
		    virq->handler[pirqhandler[hwirq].idx].vm->set_virq_pending(hwirq);
		
		    do_hwirq = true;
		}
		
		if (virq->current->state == vm_state_running || 
		    virq->current->state == vm_state_blocked)
		{	
		    dprintf(debug_virq+1, "VIRQ %d wait for preemption message from %t\n",
			    virq->mycpu, CURRENT_TID());
		    /*
		     * VM will send a preemption message, receive it.
		     */ 
		    timeouts = preemption_timeouts;
		    to = L4_nilthread;
		    continue;
		}
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
		dprintf(debug_virq, "VIRQ %d IRQ %d remote ack by %t (to %t) pirq handler %t\n",
			virq->mycpu, hwirq, from, to, pirqhandler[hwirq].virq->myself);
		
		L4_Word_t idx = tid_to_handler_idx(virq, from);
		ASSERT(idx < MAX_VIRQ_HANDLERS);
		ASSERT(pirqhandler[hwirq].virq->handler[pirqhandler[hwirq].idx].vm == 
		       virq->handler[idx].vm);
		L4_Set_VirtualSender(pirqhandler[hwirq].virq->myself);
		L4_Set_MsgTag(packtag);
	    }
	    else
	    {
		dprintf(debug_virq, "VIRQ %d IRQ %d ack by %t (to %t) pirq handler %t\n",
			virq->mycpu, hwirq, from, to, pirqhandler[hwirq].virq->myself);
		L4_Set_MsgTag(acktag);
	    }

	    pirq.global.X.thread_no = hwirq;
	    pirq.global.X.version = 1;
	    tag = L4_Reply(pirq);
	    ASSERT(!L4_IpcFailed(tag));
	    L4_Set_MsgTag(acktag);
	    continue;
	}
	break;
	case MSG_LABEL_PREEMPTION:
	{
	    /* Got a preemption messsage */
	    if (is_system_task(from))
	    {
		/* Make root servers high prio */	
		dprintf(debug_virq + 1, "VIRQ %d preempted roottask %t current %t %d\n", 
			virq->mycpu, from, CURRENT_TID(), CURRENT_STATE());	
		
		if (virq->current->state == vm_state_running)
		    virq->current->state = vm_state_blocked;
		L4_Set_MsgTag(continuetag);
		to = from;
		continue;
	    }
	    else if (from != CURRENT_TID())
	    {
		/* Kernel Operations (XCPU IPC), might have woken up somebody */
		L4_Word_t idx = tid_to_handler_idx(virq, from);
		
		if (idx >= MAX_VIRQ_HANDLERS)
		{
		    printf("VIRQ %d preempted %t (unknown VM) while %t was running (to %t)\n", 
			   virq->mycpu, from, CURRENT_TID(), to);		
		    L4_KDB_Enter("VIRQ BUG");
		}
		else
		{
		    dprintf(debug_virq, "VIRQ %d preempted %t while %t was running (to %t)\n", 
			    virq->mycpu, from, CURRENT_TID(), to);		
		    virq->handler[idx].state = vm_state_preempted;
		}
		
		/* If we've preempted during IRQs, assume TS donation, with current waiting for preemption reply */
		if (do_timer || do_hwirq)
		    virq->current->state = vm_state_preempted;
		
		to = L4_nilthread;
		continue;
	    }
	    else if (is_system_task(to))
	    {
		dprintf(debug_virq, "preempted %t after activating roottask %t\n", from, to);		
		L4_Set_MsgTag(continuetag);
		virq->current->state = vm_state_preempted;
		to = L4_nilthread;
		continue;
	    }
	    virq->current->state = vm_state_preempted;
	    reschedule = true;
	}
	break;
	case MSG_LABEL_YIELD:
	{
	    if (from != CURRENT_TID())
	    {
		dprintf(debug_virq, "yield %t while %t was running (to %t)\n", from, CURRENT_TID(), to);		
		L4_Word_t idx = tid_to_handler_idx(virq, from);
		ASSERT (idx < MAX_VIRQ_HANDLERS);
		virq->handler[idx].state = vm_state_yield;	
		virq->current->state = vm_state_preempted;	

	    }
	    else
		virq->current->state = vm_state_yield;	
	    
	    /* yield,  fetch dest */
	    L4_ThreadId_t dest;
	    L4_StoreMR(1, &dest.raw);
	    to = L4_nilthread;
	    
	    if (dest != L4_nilthread && dest != from)
	    {
		/*  verify that it's an IRQ thread on our own CPU */
		L4_Word_t idx = tid_to_handler_idx(virq, dest);
		if (idx < MAX_VIRQ_HANDLERS)
		{
		    virq->current_idx = idx;
		    virq->current = &virq->handler[idx];
		    to = virq->current->vm->get_monitor_tid(virq->current->vcpu);
		    L4_Set_MsgTag(continuetag);
		    dprintf(debug_virq, "VIRQ %d yield IPC from %t to %t\n", virq->mycpu, CURRENT_TID(), dest);
		    continue;
		}
	    }
	    dprintf(debug_virq, "VIRQ %d yield IPC from %t dest %t, reschedule\n", virq->mycpu, from, dest);
	    reschedule = true;
	}
	break;
	default:
	{
	    printf( "VIRQ %d unexpected IPC from %t current %t tag %x\n", 
		    virq->mycpu, from, CURRENT_TID(), tag.raw);
	    L4_KDB_Enter("VIRQ BUG");
	}
	break;
	}

	if (virq->num_handlers == 0)
	{
	    to = L4_nilthread; 
	    timeouts = hwirq_timeouts;
	    continue;
	}
	ASSERT(virq->current->state != vm_state_running);
	       
	if (do_timer)
	{
#if defined(VIRQ_PFREQ)
	    // MANUEL: change frequency every 10000 ticks
	    if (!(virq->ticks % 1000))
		freq_adjust_pstate();
#endif
 

	    if (migrate_vm(virq->current, virq))
	    {
		L4_Word_t dest_pcpu = (virq->mycpu + 1) % num_pcpus;
		migrate_vcpu(virq, dest_pcpu);
		
		if (virq->num_handlers == 0)
		{
		    to = L4_nilthread; 
		    timeouts = hwirq_timeouts;
		    continue;
		}
	    }
	    
	    for (L4_Word_t i = 0; i < virq->num_handlers; i++)
	    {
		/* Deliver pending virq interrupts */
		if (virq->ticks - virq->handler[i].last_tick >= 
		    virq->handler[i].period_len)
		{
		    dprintf(debug_virq + 1, "VIRQ %d deliver TIMER IRQ %d to VM %d  handler %t state %d\n",
			    virq->mycpu, 
			    ptimer_irqno_start + virq->handler[i].vcpu, i,
			    virq->handler[i].vm->get_monitor_tid(virq->handler[i].vcpu),
			    virq->handler[i].state);
		    
		    virq->handler[i].last_tick = virq->ticks;
		    virq->handler[i].vm->set_virq_pending(ptimer_irqno_start + virq->handler[i].vcpu);
		    if( virq->handler[i].state != vm_state_blocked)
			virq->handler[i].state = vm_state_preempted;
		}
	    }
	}

	if (do_hwirq)
	{
	    /* hwirq; immediately schedule handler VM
	     * TODO: bvt scheduling */	
	    
	    virq_t *hwirq_virq = pirqhandler[hwirq].virq;
	    word_t hwirq_idx = pirqhandler[hwirq].idx;

	    dprintf(debug_virq + 1, "VIRQ %d deliver  IRQ %d to VM %d  handler %t state %d\n",
		    virq->mycpu, hwirq, hwirq_idx,
		    hwirq_virq->handler[hwirq_idx].vm->get_monitor_tid(virq->current->vcpu),
		    hwirq_virq->handler[hwirq_idx].state);
	    
	    if (hwirq_virq == virq && hwirq_virq->handler[hwirq_idx].state != vm_state_blocked)
	    {
		virq->handler[hwirq_idx].state = vm_state_preempted;
		virq->current_idx = hwirq_idx;
	    }
	    
	    reschedule = false;
	}
	
	if (reschedule)
	{
	    /* Perform RR scheduling */	
	    for (L4_Word_t i = 0; i < virq->num_handlers; i++)
	    {
		virq->scheduled = (virq->scheduled + 1) % virq->num_handlers;
 		if (virq->handler[virq->scheduled].state == vm_state_preempted)
		    break;
	    }
	    
	    virq->current_idx = virq->scheduled;
	    ASSERT(virq->current_idx < virq->num_handlers);
	}
	
	do_timer = do_hwirq = false;

	virq->current = &virq->handler[virq->current_idx];
	
	if (virq->handler[virq->current_idx].state == vm_state_preempted)
	{
	    dprintf(debug_virq, "VIRQ %d dispatch new VM handler %d\n",
		    virq->mycpu, virq->current_idx);
	}
	else 
	{
	    dprintf(debug_virq, "VIRQ no running VM (current %t %d), switch to idle\n", 
		    virq->mycpu, CURRENT_TID(), CURRENT_STATE());
	     
	    idle = true;
	    to = L4_nilthread;
	    timeouts = hwirq_timeouts; 
	    continue;
	}

	to = virq->current->vm->get_monitor_tid(virq->current->vcpu);
	virq->current->state = vm_state_running;

	ASSERT(to != L4_nilthread);		
	L4_Set_MsgTag(continuetag);
	
	if (!virq->current->activated)
	{
	    dprintf(debug_virq, "VIRQ activate %t\n", to);
	    L4_Msg_t msg;
	    L4_Clear( &msg );
	    L4_Set_VirtualSender(roottask);
	    L4_Set_Propagation(&msg.tag);
	    L4_Append( &msg, virq->current->vm->binary_entry_vaddr );		// ip
	    L4_Append( &msg, virq->current->vm->binary_stack_vaddr );		// sp
	    L4_Load( &msg );
	    virq->current->activated = true;
	}
	
	if (virq->current->balance_pending)
	{
	    ASSERT(virq->current->old_pcpu < IResourcemon_max_cpus);
	    dprintf(debug_virq, "VIRQ %d propagate to %t after balance from pcpu %d tid %t\n",
		   virq->mycpu, to, virq->current->old_pcpu, virqs[virq->current->old_pcpu].myself);
		    
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


bool associate_virtual_interrupt(vm_t *vm, const L4_ThreadId_t irq_tid, 
				 const L4_ThreadId_t handler_tid, 
				 const L4_Word_t irq_cpu)
{
    /*
     * We install a virq thread that forwards IRQs and ticks with frequency 
     * 10ms / num_virq_handlers
     *  
     * for hw irqs:
     *	   hwirq is encoded in irq
     *	   pcpu id is determined by pcpu of handler
    * for timer irqs:
     *	   vcpu id (virqno) is encoded in irq no (irq = ptimer_start + virqno)
     *	   pcpu id is given
     */
    
    L4_Word_t irq = irq_tid.global.X.thread_no;
    
    if (irq < ptimer_irqno_start)
    {
	return register_hwirq_handler(vm, irq, handler_tid);
    }
    else if (irq >= ptimer_irqno_start && irq < ptimer_irqno_start + IResourcemon_max_vcpus)
    {
	L4_Word_t pcpu = irq_cpu;
	ASSERT(pcpu < IResourcemon_max_cpus);
	virq_t *virq = &virqs[pcpu];
	ASSERT(virq->mycpu == pcpu);
	L4_Word_t vcpu = irq - ptimer_irqno_start;
	
	/* jsXXX: make period len configurable */
	if (!register_timer_handler(vm, vcpu, pcpu, handler_tid, 
				    period_len, 
				    vm_state_preempted, 
				    (vcpu > 0), true))
	    return false;
	return true;
	
    }
    else
    {
	printf( "VIRQ attempt to associate invalid IRQ %d with handler %t\n", irq, handler_tid);
	return false;
    }
}


bool deassociate_virtual_interrupt(vm_t *vm, const L4_ThreadId_t irq_tid, const L4_ThreadId_t caller_tid)
{
    
    printf( "VIRQ unregistering handler unimplemented caller %t\n", caller_tid);
    L4_KDB_Enter("Resourcemon UNIMPLEMENTED");
    return false;
}

void virq_init()
{
    kip = (L4_KernelInterfacePage_t *) L4_GetKernelInterface ();
    roottask = L4_Myself();
    roottask_local = L4_MyLocalId();
    s0 = L4_GlobalId (kip->ThreadInfo.X.UserBase, 1);
    num_pcpus = L4_NumProcessors(kip);
    ptimer_irqno_start = L4_ThreadIdSystemBase(kip) - num_pcpus;
    user_base = L4_ThreadIdUserBase(kip);
    
    /* PIC timer = 2ms, APIC = 1ms */
    if (ptimer_irqno_start == 16)
	period_len = 5;
    else
	period_len = 10;

    dprintf(debug_virq, "VTimer period len %d\n", period_len);

    if (!L4_Set_Priority(s0, PRIO_ROOTSERVER))
    {
	printf("Error: unable to set SIGMA0's prio to %d, L4 error code: %d\n", 
		PRIO_ROOTSERVER, L4_ErrorCode());
	L4_KDB_Enter("VIRQ BUG");
    }
    
    if (!L4_Set_Priority(roottask, PRIO_ROOTSERVER))
    {
	printf("Error: unable to set ROOTTASK's prio to %d, L4 error code: %d\n", 
		PRIO_ROOTSERVER, L4_ErrorCode());
	L4_KDB_Enter("VIRQ BUG");
    }


#if defined(VIRQ_PFREQ)
    powernow_init(min(IResourcemon_max_cpus,L4_NumProcessors(kip))); // MANUEL
#endif

    dummy_handler.vm = NULL;
    dummy_handler.vcpu = IResourcemon_max_vcpus;
    dummy_handler.period_len = 0;
    dummy_handler.last_tick = 0;
    dummy_handler.balance_pending = false;    
    dummy_handler.old_pcpu = IResourcemon_max_cpus;
    dummy_handler.last_balance = 0;
    dummy_handler.activated = false;
    dummy_handler.state = vm_state_invalid;
    
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
	    virq->handler[h_idx].activated = false;
	    virq->handler[h_idx].state = vm_state_preempted;
	}
	
	virq->ticks = 0;
	virq->num_handlers = 0;
	virq->current = &dummy_handler;
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
	    printf( "Could not install virq TID: %t\n", virq->thread->get_global_tid());
    
	    return;
	} 

		if (!L4_Set_ProcessorNo(virq->thread->get_global_tid(), cpu))
	{
	    printf("Error: unable to set virq thread's cpu to %d L4 error code: %d\n",
		    cpu, L4_ErrorCode());
	    return;
	}
	
	virq->myself = virq->thread->get_global_tid();
	virq->mycpu = cpu;
	
	L4_ThreadId_t ptimer = L4_nilthread;
	ptimer.global.X.thread_no = ptimer_irqno_start + virq->mycpu;
	ptimer.global.X.version = 1;
	L4_Start( virq->thread->get_global_tid() ); 
	associate_ptimer(ptimer, virq);

	
	if (cpu == 0)
	{
	    if (!L4_ThreadControl (s0, s0, virq->thread->get_global_tid(), s0, (void *) (-1)))
	    {
		printf( "Error: unable to set SIGMA0's  scheduler to %t L4 error code: %d\n",
			virq->thread->get_global_tid(), L4_ErrorCode());    
 		L4_KDB_Enter("VIRQ BUG");
	    }
	    if (!L4_ThreadControl (roottask, roottask, virq->thread->get_global_tid(), s0, (void *) -1))
	    {
		printf( "Error: unable to set ROOTTASK's  scheduler to %t L4 error code: %d\n",
			virq->thread->get_global_tid(), L4_ErrorCode());    
		L4_KDB_Enter("VIRQ BUG");
	    }
	}
    }
}
    


#endif /* defined(cfg_l4ka_vmextensions) */
