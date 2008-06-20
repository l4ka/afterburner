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
vm_context_t dummy_vm;

pirqhandler_t pirqhandler[MAX_IRQS];
L4_Word_t ptimer_irqno_start;


static const L4_Word_t hwirq_timeouts = L4_Timeouts(L4_ZeroTime, L4_Never);
static const L4_Word_t preemption_timeouts = L4_Timeouts(L4_ZeroTime, L4_ZeroTime);

#define MSG_LABEL_PREEMPTION	0xffd0
#define MSG_LABEL_YIELD		0xffd1
#define MSG_LABEL_ACTIVATION	0xffd2
#define MSG_LABEL_IDLE		0xffd3
#define MSG_LABEL_CONTINUE	0xffd4

#define MSG_LABEL_IRQ		0xfff0
#define MSG_LABEL_IRQ_ACK	0xfff1

const L4_MsgTag_t continuetag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_CONTINUE} }; 
const L4_MsgTag_t pcontinuetag = (L4_MsgTag_t) { X: { 0, 0, 1, MSG_LABEL_CONTINUE} }; 
const L4_MsgTag_t acktag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_IRQ_ACK} }; ;
const L4_MsgTag_t packtag = (L4_MsgTag_t) { X: { 0, 0, 1, MSG_LABEL_IRQ_ACK} }; ; 

L4_Word_t period_len, num_pcpus;

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


static inline bool is_irq_tid(L4_ThreadId_t tid)
{
    ASSERT(user_base);
    return (tid.global.X.thread_no < user_base);
    
}

static inline bool is_system_task(L4_ThreadId_t tid)
{
    ASSERT(user_base);
    ASSERT(L4_IsGlobalId(tid));
    return (tid != L4_nilthread && 
	    (is_irq_tid(tid) || tid == s0 || tid == roottask));
}

static inline L4_Word_t tid_to_vm_idx(virq_t *virq, L4_ThreadId_t tid)
{
    ASSERT(L4_IsGlobalId(tid));
    for (word_t idx=0; idx < virq->num_vms; idx++)
    {
	if (virq->vctx[idx].monitor_tid == tid)
	    return idx;
    }
    return MAX_VIRQ_VMS;
		
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
	ASSERT(pirqhandler[hwirq].idx != MAX_VIRQ_VMS);
	vm_t *pirq_vm = virq->vctx[pirqhandler[hwirq].idx].vm;
	    
	dprintf(debug_virq-2, "VIRQ %d already registered to handler %t override with handler %t\n", 
		hwirq, pirq_vm->get_monitor_tid(virq->vctx[pirqhandler[hwirq].idx].vcpu),
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

	
    L4_Word_t vm_idx = tid_to_vm_idx(virq, handler_tid);
    ASSERT(vm_idx < MAX_VIRQ_VMS);
	
    pirqhandler[hwirq].virq = virq;
    pirqhandler[hwirq].idx = vm_idx;

    return true;
}

static inline vm_context_t *register_timer_handler(vm_t *vm, word_t vcpu, word_t pcpu, 
						     L4_ThreadId_t handler_tid, 
						     word_t period_len, vm_state_e state,
						     bool started, bool failable)
{
    
    ASSERT(pcpu < IResourcemon_max_cpus);
    ASSERT(vcpu < IResourcemon_max_vcpus);
    
    virq_t *virq = &virqs[pcpu];
    ASSERT(virq->mycpu == pcpu);

    if (virq->num_vms == MAX_VIRQ_VMS)
    {
	printf( "VIRQ reached maximum number of handlers (%x)\n", virq->num_vms);
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

    vm_context_t *handler = &virq->vctx[virq->num_vms];
    vm->set_virq_tid(virq->mycpu, virq->thread->get_global_tid());
    vm->set_monitor_tid(vcpu, handler_tid);
    vm->set_pcpu(vcpu, virq->mycpu);
	

    handler->vm = vm;
    handler->vcpu = vcpu;
    handler->state = state;
    handler->period_len = period_len;
    handler->started = started;
    handler->monitor_tid = handler_tid;
    

    dprintf(debug_virq, "VIRQ %d register vctx %d handler %t pTIMER %d vTIMER %d period len %d\n",
	    virq->mycpu, virq->num_vms, handler_tid, ptimer_irqno_start + virq->mycpu, 
	    ptimer_irqno_start + vcpu, period_len);
    
    virq->num_vms++;

    return handler;
}


static inline bool unregister_timer_handler(L4_Word_t vm_idx)
{
    virq_t *virq = &virqs[L4_ProcessorNo()];

    vm_t *vm = virq->vctx[vm_idx].vm;
    L4_Word_t vcpu = virq->vctx[vm_idx].vcpu;
    
    ASSERT(vm);
    ASSERT(vcpu < IResourcemon_max_vcpus);
    
    dprintf(debug_virq, "VIRQ deassociate TIMER IRQ %d with handler %t virq_tid %t pcpu %d\n",
	    ptimer_irqno_start + vcpu, vm->get_monitor_tid(vcpu), 
	    virq->thread->get_global_tid(), virq->mycpu);

    vm->set_monitor_tid(vcpu, L4_nilthread);
    vm->set_pcpu(vcpu, IResourcemon_max_cpus);

    /* Compact list */
    for (L4_Word_t idx = vm_idx; idx < virq->num_vms-1; idx++)
    {
	virq->vctx[idx].vm = virq->vctx[idx+1].vm;
	virq->vctx[idx].vcpu = virq->vctx[idx+1].vcpu;
	virq->vctx[idx].state = virq->vctx[idx+1].state;
	virq->vctx[idx].period_len = virq->vctx[idx+1].period_len;
	virq->vctx[idx].last_tick = virq->vctx[idx+1].last_tick;
	virq->vctx[idx].last_balance = virq->vctx[idx+1].last_balance;
	virq->vctx[idx].old_pcpu = virq->vctx[idx+1].old_pcpu;
	virq->vctx[idx].balance_pending = virq->vctx[idx+1].balance_pending;
    }	

    virq->vctx[virq->num_vms].vm = NULL;
    virq->vctx[virq->num_vms].vcpu = IResourcemon_max_vcpus;
    virq->vctx[virq->num_vms].period_len = 0;
    virq->vctx[virq->num_vms].last_tick = 0;
    virq->vctx[virq->num_vms].last_balance = 0;
    virq->vctx[virq->num_vms].old_pcpu = IResourcemon_max_cpus;
    virq->vctx[virq->num_vms].balance_pending = false;
    virq->vctx[virq->num_vms].state = vm_state_runnable;

    virq->num_vms--;

    if (virq->num_vms == 0)
	virq->current = &dummy_vm;

    return true;
}

static inline vm_context_t *register_system_task(word_t pcpu, L4_ThreadId_t tid, vm_state_e state)
{
    ASSERT(pcpu < IResourcemon_max_cpus);
    
    virq_t *virq = &virqs[pcpu];
    ASSERT(virq->mycpu == pcpu);
	   
    if (virq->num_vms == MAX_VIRQ_VMS)
    {
	printf( "VIRQ reached maximum number of handlers (%x)\n", virq->num_vms);
	return NULL;
    }

    L4_Word_t dummy;
    if (!L4_Schedule(tid, virq->myself.raw, (1 << 16 | virq->mycpu), ~0UL, ~0, &dummy))
    {
	L4_Word_t errcode = L4_ErrorCode();
	
	printf("VIRQ failed to set scheduling parameters of system task %t error %d\n", tid, errcode);
	
	return NULL;
    }

    vm_context_t *handler = &virq->vctx[virq->num_vms];

    handler->vm = NULL;
    handler->vcpu = pcpu;
    handler->state = state;
    handler->period_len = 0;
    handler->started = true;
    handler->system_task = true;
    handler->monitor_tid = tid;
    
    virq->num_vms++;
	
    dprintf(debug_virq, "VIRQ %d register system vctx %d handler %t\n",
	    virq->mycpu, virq->num_vms - 1, handler->monitor_tid);
    
    return handler;
}

static inline void init_root_servers(virq_t *virq)
{
    L4_Word_t dummy;
    L4_Word_t time_control = virq->myself.raw;
    L4_Word_t processor_control = (1 << 16 | virq->mycpu);
    L4_Word_t preemption_control = (3 << 24);

    if (!L4_Schedule(s0, time_control, processor_control, ~0UL, preemption_control, &dummy))
    {
	printf( "Error: unable to set SIGMA0's scheduler, L4 error code: %d\n", L4_ErrorCode());
	L4_KDB_Enter("VIRQ BUG");
    }
    if (!L4_Schedule(roottask, time_control, processor_control, ~0UL, preemption_control, &dummy))
    {
	printf( "Error: unable to set ROOTTASK's scheduler, L4 error code: %d\n", L4_ErrorCode());
	L4_KDB_Enter("VIRQ BUG");
    }
    setup_thread_faults(s0);
    setup_thread_faults(roottask);
    
    register_system_task(virq->mycpu, roottask, vm_state_runnable);
    register_system_task(virq->mycpu, s0, vm_state_blocked);
    
    virq->current_idx = 0;
    virq->current = &virq->vctx[virq->current_idx];

    
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
static inline bool migrate_vm(vm_context_t *handler, virq_t *virq)
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
	state = vm_state_runnable;
    }

    vm_context_t *handler = register_timer_handler(vm, vcpu, dest_pcpu, tid, period_len, 
						     state, true, false); 
    ASSERT(handler);
    handler->balance_pending = true;    
    handler->old_pcpu = virq->mycpu;
    handler->last_balance = virqs[dest_pcpu].ticks; // MANUEL: set last_balance to the current "time" of the NEW pcpu
}


static inline void parse_pmipc_logfile(word_t cpu, word_t domain)
{
    //L4_Word64_t most_recent_timestamp = 0;
    L4_Word_t count = 0;
    
    /*
     * Get logfile
     */
    u16_t *s = L4_SELECTOR(cpu, domain, L4_LOGGING_RESOURCE_PMIPC);
    
    logfile_control_t *c = l4_logfile_base[cpu] + (*s / sizeof(logfile_control_t)) ;
    volatile L4_Word_t *current_idx = (L4_Word_t* ) (c + (c->X.current_offset / sizeof(logfile_control_t)));
    
    printf( "VIRQ pmipc selector %x ctrl reg %x current_idx @ %x = %x\n",
	    s, c, current_idx, *current_idx);
    
    if (L4_LOGGING_LOG_ENTRIES(c)==0) {
	printf( "No log entries found for CPU %d, break!\n", cpu);
	return;	    
    }
    
    if (L4_LOGGING_LOG_ENTRIES(c)>128) {
	printf( "Too many log entries (%d) found! Log may be in corrupt state. Abort!\n",
		L4_LOGGING_LOG_ENTRIES(c));
	return;
    }
    
    while (count <= (L4_LOGGING_LOG_ENTRIES(c)-32))
    {
	/*
	 * Logfile of preempted threads domain contains 
	 *	0       tid
	 * 	1	preempter
	 *	2	time hi
	 * 	3	time low 
	 *	4-14	GPREGS item
	 *	15	dummy
	 * read all state
	 */
	L4_ThreadId_t tid, preempter;
	tid.raw =  *current_idx;
	L4_LOGGING_DEC_LOG(current_idx, c);
	preempter.raw = *current_idx;
	L4_LOGGING_DEC_LOG(current_idx, c);

	printf("log says %t preempted by %t\n\t state: <" );

	for (L4_Word_t ctr=0; ctr < 2; ctr++)
	{
	    word_t s = *current_idx;
	    L4_LOGGING_DEC_LOG(current_idx, c);
	    printf("%x:", s);
	    s = *current_idx;
	    L4_LOGGING_DEC_LOG(current_idx, c);
	    printf("%x:", s);
	    s = *current_idx;
	    L4_LOGGING_DEC_LOG(current_idx, c);
	    printf("%x:", s);
	    s = *current_idx;
	    L4_LOGGING_DEC_LOG(current_idx, c);
	    printf("%x:", s);
	    s = *current_idx;
	    L4_LOGGING_DEC_LOG(current_idx, c);
	    printf("%x>\n\t<", s);
	}

    }
}
static void virq_thread( 
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
    
    virq_t *virq = &virqs[L4_ProcessorNo()];
    L4_ThreadId_t from = L4_nilthread;
    L4_ThreadId_t rfrom = L4_nilthread;
    L4_ThreadId_t current = L4_nilthread;
    L4_ThreadId_t ptimer = L4_nilthread;
    L4_ThreadId_t pirq = L4_nilthread;
    L4_ThreadId_t preempter = L4_nilthread;
    L4_ThreadId_t rpreempter = L4_nilthread;
    L4_ThreadId_t activatee = L4_nilthread;
    L4_ThreadId_t ractivatee = L4_nilthread;
    L4_MsgTag_t tag;
    L4_Word_t hwirq = 0;
    
    ASSERT(virq->myself == L4_Myself());
    ASSERT(virq->mycpu == L4_ProcessorNo());
    
    if (virq->mycpu == 0)
	init_root_servers(virq);


    ptimer.global.X.thread_no = ptimer_irqno_start + virq->mycpu;
    ptimer.global.X.version = 1;
    
    bool do_timer = false, do_hwirq = false;
    bool reschedule = false;
    bool idle = false;

    current = virq->current->monitor_tid;
    ASSERT(virq->current->system_task);
    
    dprintf(debug_virq, "VIRQ %d started current %t\n", virq->mycpu, current);
    L4_KDB_Enter("VIRQ INIT");
    
    tag = L4_Wait(&from);
    
    for (;;)
    {
	if (L4_IpcFailed(tag))
	{
	    if ((L4_ErrorCode() & 0xf) == 3)
 		printf("VIRQ %d receive timeout to %t current %t\n", 
		       virq->mycpu, current);
	    else
		printf("VIRQ %d send timeout to %t current  %t\n", 
		       virq->mycpu, current);
	    L4_KDB_Enter("VIRQ BUG");
	}

	if (L4_IsLocalId(from))
	{
	    ASSERT(from == roottask_local);
	    from = roottask;
	}

	switch( L4_Label(tag) )
	{
	case MSG_LABEL_IDLE:
	{
	    
	    ASSERT(from == current);
	    
	    dprintf(debug_virq + 1, "VIRQ %d idle IPC current %t num VMs %d\n", 
		    virq->mycpu, current, virq->num_vms);
	    
	    ASSERT(virq->num_vms);
	    ASSERT(virq->current->state == vm_state_runnable);
	    
	    virq->current->state = vm_state_blocked;
	    reschedule = true;
	}
	break;
	case MSG_LABEL_PREEMPTION:
	{
	    //	preemption		-> IPC to lcs(current, preempter)
	    //	 virtualsender	 = current's VM tid
	    //	 actualsender	 = current
	    //	 mr1		 = preempter's VM tid 
	    //	 mr2		 = preempter
	    //	 ctrlxfer item of current
	    
	    // Get the preempter's VM and real preempters  TID
	    L4_StoreMR(1, &preempter.raw);
	    L4_StoreMR(2, &rpreempter.raw); 

	    if (is_irq_tid(from))
	    {
		// current preeempted by an irq, swap current and preempter
		ASSERT(preempter == current || preempter == virq->myself);
		preempter = rpreempter = from;
		from = current;
	    }

	    ASSERT(from == current);
	    

	    if (L4_IpcPropagated (tag))
		virq->current->current_tid = L4_ActualSender();
	    else
		virq->current->current_tid = from;
	    
	    ASSERT (virq->current->state == vm_state_runnable);
	    
	    if (rpreempter != preempter)
		L4_KDB_Enter("UNTESTED HIERARCHICAL PREEMPTION");
	    
	    if (preempter == ptimer)
	    {
		dprintf(debug_virq + 1, "VIRQ %d preempted current %t state %d by timer\n", 
			virq->mycpu, current);
		
		do_timer = true;
	    }
	    else if (is_irq_tid(preempter))
	    {
		dprintf(debug_virq, "VIRQ %d preempted current %t by irq %d\n", 
			    virq->mycpu, current, hwirq);
		    hwirq = preempter.global.X.thread_no;
		    do_hwirq = true;
	    }
	    else
	    {
		ASSERT(preempter == virq->myself);
	    }
	}
	break;
	case MSG_LABEL_YIELD:
	{
	    ASSERT(from == current);
	    virq->current->state = vm_state_yield;	
	    
	    /* yield,  fetch dest */
	    L4_ThreadId_t dest;
	    L4_StoreMR(1, &dest.raw);
	    current = L4_nilthread;
	    
	    if (dest != L4_nilthread && dest != current)
	    {
		/*  verify that it's an IRQ thread on our own CPU */
		L4_Word_t idx = tid_to_vm_idx(virq, dest);
		if (idx < MAX_VIRQ_VMS)
		{
		    virq->current_idx = idx;
		    virq->current = &virq->vctx[idx];
		    current = virq->current->vm->get_monitor_tid(virq->current->vcpu);
		    L4_Set_MsgTag(continuetag);
		    dprintf(debug_virq, "VIRQ %d yield IPC from %t to %t\n", virq->mycpu, current, dest);
		    L4_KDB_Enter("VIRQ UNTESTED YIELD WITH DEST");

		}
	    }
	    else 
	    {
		dprintf(debug_virq, "VIRQ %d yield IPC current %t dest %t, reschedule\n", virq->mycpu, current, dest);
		reschedule = true;
		L4_KDB_Enter("VIRQ UNTESTED YIELD");
	    }

	}
	break;
	case MSG_LABEL_ACTIVATION:
	{
	    ASSERT(from == current);
	    
 	    // Activation:
	    //  virtualsender = activator's (from) VM tid
	    //  actualsender  = activatee
	    //    mr1		 = activatee's VM tid
	    //    mr2		 = activator
	    //    mr3		 = preempter's VM tid
	    //    mr4		 = preempter

	    ASSERT(L4_IpcPropagated(tag));
		   
	    ractivatee = L4_ActualSender();
	    L4_StoreMR(1, &activatee.raw); 
	    L4_StoreMR(2, &rfrom.raw); 
	    L4_StoreMR(3, &preempter.raw); 
	    L4_StoreMR(4, &rpreempter.raw); 
    
 
	    ASSERT (virq->current->state == vm_state_runnable);
	    virq->current->state = vm_state_blocked;
	    
	    L4_Word_t idx = tid_to_vm_idx(virq, activatee);
	    ASSERT(idx < MAX_VIRQ_VMS);
	    ASSERT(virq->vctx[idx].state == vm_state_blocked);
	    
	    virq->vctx[idx].state = vm_state_runnable;
	    virq->vctx[idx].current_tid = ractivatee;
	    
	    if (preempter != L4_nilthread)
	    {
		idx = tid_to_vm_idx(virq, preempter);
		printf("activation with preempter %t rpreempter %t\n", 
		       preempter, rpreempter);
		ASSERT(idx < MAX_VIRQ_VMS);
		ASSERT(virq->vctx[idx].state == vm_state_blocked);
		virq->vctx[idx].state = vm_state_runnable;
		virq->vctx[idx].current_tid = rpreempter;
	    }
	    
	    // Perform handoff by switching current
	    virq->current_idx = idx;
	    virq->current = &virq->vctx[idx];
	    current = ractivatee;

	    if (activatee != ractivatee)
	    {
		L4_KDB_Enter("UNTESTED HIERARCHICAL ACTIVATION");
	    }

	}
	break;
	case MSG_LABEL_IRQ_ACK:
	{
	    ASSERT(from == current);
	    L4_KDB_Enter("VIRQ IRQ ACK UNTESTED");	    
	    L4_StoreMR( 1, &hwirq );
	    ASSERT( hwirq < ptimer_irqno_start );
    
	    if (L4_IpcPropagated(L4_MsgTag())) current = L4_ActualSender();

	    /* Verify that sender belongs to associated VM */
	    if (pirqhandler[hwirq].virq != virq)
	    {
		dprintf(debug_virq, "VIRQ %d IRQ %d remote ack by %t pirq handler %t\n",
			virq->mycpu, hwirq, current, pirqhandler[hwirq].virq->myself);
		
		L4_Word_t idx = tid_to_vm_idx(virq, current);
		ASSERT(idx < MAX_VIRQ_VMS);
		ASSERT(pirqhandler[hwirq].virq->vctx[pirqhandler[hwirq].idx].vm == 
		       virq->vctx[idx].vm);
		L4_Set_VirtualSender(pirqhandler[hwirq].virq->myself);
		L4_Set_MsgTag(packtag);
	    }
	    else
	    {
		dprintf(debug_virq, "VIRQ %d IRQ %d ack by  %t pirq handler %t\n",
			virq->mycpu, hwirq, current, pirqhandler[hwirq].virq->myself);
		L4_Set_MsgTag(acktag);
	    }

	    pirq.global.X.thread_no = hwirq;
	    pirq.global.X.version = 1;
	    tag = L4_Reply(pirq);
	    ASSERT(!L4_IpcFailed(tag));
	    L4_Set_MsgTag(acktag);
	}
	break;
	default:
	{
	    printf( "VIRQ %d unexpected msg current %t tag %x\n", 
		    virq->mycpu, current, tag.raw);
	    L4_KDB_Enter("VIRQ BUG");
	}
	break;
	}

	ASSERT(virq->num_vms);
	
#if defined(cfg_eacc)
	for (L4_Word_t domain = 0; domain <= vm_t::max_domain_in_use; domain++)
	    parse_pmipc_logfile(virq->mycpu, domain);
#endif

	if (do_timer)
	{
	    virq->ticks++;
#if defined(LATENCY_BENCHMARK)
	    virq_latency_benchmark(virq);
#endif
#if defined(VIRQ_PFREQ)
	    // MANUEL: change frequency every 10000 ticks
	    if (!(virq->ticks % 1000))
		freq_adjust_pstate();
#endif
		
	    /* Acknlowledge Timer */
	    L4_Set_MsgTag(acktag);
	    tag = L4_Reply(ptimer);
	    ASSERT(!L4_IpcFailed(tag));		
		

	    if (migrate_vm(virq->current, virq))
	    {
		L4_Word_t dest_pcpu = (virq->mycpu + 1) % num_pcpus;
		migrate_vcpu(virq, dest_pcpu);
		
		ASSERT (virq->num_vms);
	    }
	    
	    for (L4_Word_t i = 0; i < virq->num_vms; i++)
	    {
		if (virq->vctx[i].system_task)
		    continue;
		
		/* Deliver pending virq interrupts */
		if (virq->ticks - virq->vctx[i].last_tick >= 
		    virq->vctx[i].period_len)
		{
		    dprintf(debug_virq + 1, "VIRQ %d deliver TIMER IRQ %d to VM %d current_tid %t state %d\n",
			    virq->mycpu, 
			    ptimer_irqno_start + virq->vctx[i].vcpu, i,
			    virq->vctx[i].current_tid,
			    virq->vctx[i].state);
		    
		    /* UNIMPLEMENTED HIERARCHICAL IRQ DELIVERY */
		    ASSERT(virq->vctx[i].current_tid == virq->vctx[i].vm->get_monitor_tid(virq->current->vcpu));
		    
		    virq->vctx[i].last_tick = virq->ticks;
		    virq->vctx[i].vm->set_virq_pending(ptimer_irqno_start + virq->vctx[i].vcpu);
		    
		    if( virq->vctx[i].state == vm_state_yield)
			virq->vctx[i].state = vm_state_runnable;
		}
	    }
	    reschedule = true;
	}

	if (do_hwirq)
	{
	    ASSERT(hwirq < user_base);
	    ASSERT(hwirq < ptimer_irqno_start);
	    
	    word_t hwirq_idx = pirqhandler[hwirq].idx;
	    ASSERT(hwirq_idx < MAX_VIRQ_VMS);
	    
	    virq->vctx[hwirq_idx].vm->set_virq_pending(hwirq);

	    /* hwirq; immediately schedule handler VM
	     * TODO: bvt scheduling */	
	    
	    virq_t *hwirq_virq = pirqhandler[hwirq].virq;
	    
	    dprintf(debug_virq + 1, "VIRQ %d deliver  IRQ %d to VM %d current_tid %t state %d\n",
		    virq->mycpu, hwirq, hwirq_idx,
		    hwirq_virq->vctx[hwirq_idx].current_tid,
		    hwirq_virq->vctx[hwirq_idx].state);

	    /* UNIMPLEMENTED HIERARCHICAL IRQ DELIVERY */
	    ASSERT(hwirq_virq->vctx[hwirq_idx].current_tid ==
		   hwirq_virq->vctx[hwirq_idx].vm->get_monitor_tid(virq->current->vcpu));
	    
	    if (hwirq_virq == virq && hwirq_virq->vctx[hwirq_idx].state != vm_state_yield)
	    {
		virq->vctx[hwirq_idx].state = vm_state_runnable;
		virq->current_idx = hwirq_idx;
	    }
	    reschedule = false;
	}
	
	if (reschedule)
	{
	    /* Perform RR scheduling */	
	    for (L4_Word_t i = 0; i < virq->num_vms; i++)
	    {
		virq->scheduled = (virq->scheduled + 1) % virq->num_vms;
 		if (virq->vctx[virq->scheduled].state == vm_state_runnable)
		    break;
	    }
	    
	    virq->current_idx = virq->scheduled;
	    ASSERT(virq->current_idx < virq->num_vms);
	}
	
	do_timer = do_hwirq = false;

	virq->current = &virq->vctx[virq->current_idx];
	
	if (virq->vctx[virq->current_idx].state == vm_state_runnable)
	{
	    dprintf(debug_virq+1, "VIRQ %d dispatch new VM handler %d\n",
		    virq->mycpu, virq->current_idx);
	    
	    current = virq->current->monitor_tid;
	    XXX
	    L4_Set_MsgTag(continuetag);
	    
	}
	else 
	{
	    dprintf(debug_virq+1, "VIRQ no running VM (current %t), switch to idle\n", 
		    virq->mycpu, current);
	     
	    idle = true;
	    dprintf(debug_virq+1, "VIRQ %d idle current %t\n", virq->mycpu, current);
	    L4_KDB_Enter("VIRQ UNTESTED IDLE");
	    __asm__ __volatile__ ("hlt" ::: "memory" );
	    L4_KDB_Enter("VIRQ UNIMPLEMENTED IDLE WAKEUP");
	    continue;
	}
	
	idle = false;
	reschedule = false;

	if (!virq->current->started)
	{
	    dprintf(debug_virq + 1, "VIRQ start %t\n", current);
	    virq->current->started = false;
	    L4_Msg_t msg;
	    L4_Clear( &msg );
	    L4_Set_VirtualSender(roottask);
	    L4_Set_Propagation(&msg.tag);
	    L4_Append( &msg, virq->current->vm->binary_entry_vaddr );		// ip
	    L4_Append( &msg, virq->current->vm->binary_stack_vaddr );		// sp
	    L4_Load( &msg );
	}
	else if (virq->current->balance_pending)
	{
	    ASSERT(virq->current->old_pcpu < IResourcemon_max_cpus);
	    dprintf(debug_virq, "VIRQ %d propagate to current %t after balance from pcpu %d tid %t\n",
		    virq->mycpu, current, virq->current->old_pcpu, virqs[virq->current->old_pcpu].myself);
		    
	    L4_Set_VirtualSender(virqs[virq->current->old_pcpu].myself);
	    L4_Set_MsgTag(pcontinuetag);
	    virq->current->old_pcpu = IResourcemon_max_cpus;
	    virq->current->balance_pending = false;
	    virq->current->last_balance = virq->ticks;
    
	}
	
	tag = L4_ReplyWait (current, &from);
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
				    vm_state_runnable, 
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

    dummy_vm.vm = NULL;
    dummy_vm.vcpu = IResourcemon_max_vcpus;
    dummy_vm.period_len = 0;
    dummy_vm.last_tick = 0;
    dummy_vm.balance_pending = false;    
    dummy_vm.old_pcpu = IResourcemon_max_cpus;
    dummy_vm.last_balance = 0;
    dummy_vm.started = false;
    dummy_vm.monitor_tid = L4_nilthread;
    dummy_vm.last_tid = L4_nilthread;
    dummy_vm.current_tid = L4_nilthread;
    dummy_vm.state = vm_state_invalid;
    
    for (L4_Word_t cpu=0; cpu < min(IResourcemon_max_cpus,L4_NumProcessors(kip)); cpu++)
    {
	virq_t *virq = &virqs[cpu];
	
	for (L4_Word_t v_idx=0; v_idx < MAX_VIRQ_VMS; v_idx++)
	{
	    virq->vctx[v_idx].vm = NULL;
	    virq->vctx[v_idx].vcpu = IResourcemon_max_vcpus;
	    virq->vctx[v_idx].period_len = 0;
	    virq->vctx[v_idx].last_tick = 0;
	    virq->vctx[v_idx].balance_pending = false;    
	    virq->vctx[v_idx].old_pcpu = IResourcemon_max_cpus;
	    virq->vctx[v_idx].last_balance = 0;
	    virq->vctx[v_idx].started = false;
	    virq->vctx[v_idx].current_tid = L4_nilthread;
	    virq->vctx[v_idx].last_tid = L4_nilthread;
	    virq->vctx[v_idx].monitor_tid = L4_nilthread;
	    virq->vctx[v_idx].state = vm_state_invalid;
	}
	
	virq->ticks = 0;
	virq->num_vms = 0;
	virq->current = &dummy_vm;
	virq->scheduled = 0;
	
	for (L4_Word_t irq=0; irq < MAX_IRQS; irq++)
	{
	    pirqhandler[irq].idx = MAX_VIRQ_VMS;
	    pirqhandler[irq].virq = NULL;
	}
	virq->thread = NULL;
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
