/*********************************************************************
 *                
 * Copyright (C) 2006-2010,  Karlsruhe University
 *                
 * File path:     l4ka-resourcemon/virq.cc
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
#include <ia32/msr.h>
#include <math.h>
#include <basics.h>
#include <debug.h>
#include <hthread.h>
#include <vm.h>
#include <virq.h>
#include <resourcemon.h>
#include <freq_powernow.h>
#include <freq_scaling.h>
#include <earm.h>

#undef VIRQ_PFREQ

#undef VIRQ_BALANCE
#define VIRQ_BALANCE_INTERVAL_MS	(10)
#define VIRQ_BALANCE_DEBUG 1

L4_ThreadId_t roottask = L4_nilthread;
L4_ThreadId_t roottask_local = L4_nilthread;
L4_ThreadId_t s0 = L4_nilthread;

virq_t virqs[IResourcemon_max_cpus];
vm_context_t dummy_vm;

pirqhandler_t pirqhandler[MAX_IRQS];
L4_Word_t ptimer_irqno_start;

const char *vctx_state_string[4] = 
{ "RNG ", "YLD ", "BLK ", "INV " };

const char *virq_scheduler_string[3] = 
{ "EASA", "EASV", "TIME" };

static const L4_Word_t hwirq_timeouts = L4_Timeouts(L4_ZeroTime, L4_Never);
static const L4_Word_t preemption_timeouts = L4_Timeouts(L4_ZeroTime, L4_ZeroTime);

#define MSG_LABEL_PREEMPTION	0xffd0
#define MSG_LABEL_YIELD		0xffd1
#define MSG_LABEL_ACTIVATION	0xffd2
#define MSG_LABEL_BLOCKED	0xffd3
#define MSG_LABEL_CONTINUE	0xffd4

#define MSG_LABEL_IRQ		0xfff0
#define MSG_LABEL_IRQ_ACK	0xfff1

#define MSG_LABEL_EARM		0xfff2

const L4_MsgTag_t continuetag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_CONTINUE} }; 
const L4_MsgTag_t pcontinuetag = (L4_MsgTag_t) { X: { 0, 0, 1, MSG_LABEL_CONTINUE} }; 
const L4_MsgTag_t acktag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_IRQ_ACK} }; ;
const L4_MsgTag_t packtag = (L4_MsgTag_t) { X: { 0, 0, 1, MSG_LABEL_IRQ_ACK} }; ; 

const L4_ThreadId_t idle_tid = { raw: 0x1d1e1d1e };
L4_Word_t period_len, tick_ms;

void virq_earm_update(virq_t *virq)
{
#if defined(cfg_earm)
    L4_Word64_t pmcstate[8];
    L4_Word64_t energy, tsc;
    
    earmcpu_pmc_snapshot(pmcstate);
    earmcpu_update(virq->mycpu, virq->current->logid, pmcstate, virq->pmcstate, &energy, &tsc);	

    dprintf(debug_earm+1, "VIRQ %d VM %d energy %d tsc %d\n", 
	    virq->mycpu, virq->current_idx, (L4_Word_t) energy, (L4_Word_t) tsc);

    if (virq->current_idx)
    {
	virq->stsc += tsc;    
	virq->senergy += energy;	
    }
    
    virq->current->stsc += tsc;
    virq->current->senergy += energy;					

    for (L4_Word_t i=0; i < 8; i++)
        virq->pmcstate[i] = pmcstate[i];
#endif
  
};


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
	dprintf(debug_virq, "VIRQ %d already registered to handler %t override with handler %t\n", 
		hwirq, virq->vctx[pirqhandler[hwirq].idx].vm->get_monitor_tid(virq->vctx[pirqhandler[hwirq].idx].vcpu),
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
	
	setup_thread_faults(irq_tid);
	
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
	dprintf(debug_virq, "VIRQ monitor tid %t vcpu %d already registered for pcpu %d\n", 
		handler_tid , vcpu, vm->get_pcpu(vcpu));
	return NULL;
    }
   
    L4_Word_t dummy;
    if (!L4_Schedule(handler_tid, virq->myself.raw, (1 << 16 | virq->mycpu), ~0UL, ~0, &dummy))
    {
	dprintf(debug_virq - (failable ? 0 : 3),
		"VIRQ failed to set scheduling parameters of handler %t error %d (migration in progress?)\n",
		handler_tid, L4_ErrorCode());
	
	return NULL;
    }

    vm_context_t *handler = &virq->vctx[virq->num_vms];
    vm_context_init(&virq->vctx[virq->num_vms]);
    
    vm->set_virq_tid(virq->mycpu, virq->thread->get_global_tid());
    vm->set_monitor_tid(vcpu, handler_tid);
    vm->set_pcpu(vcpu, virq->mycpu);
    handler->vm = vm;
    handler->logid =  L4_LOG_ROOTSERVER_LOGID + vm->get_space_id();
    handler->vcpu = vcpu;
    handler->state = state;
    handler->period_len = period_len;
    handler->started = started;
    handler->monitor_tid = handler_tid;
    handler->last_tid = handler_tid;
    handler->ticket = 10;
    
    setup_thread_faults(handler_tid);

    set_max_logid_in_use(handler->logid);

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
	u8_t *src = (u8_t *) &virq->vctx[idx+1];
	u8_t *dst = (u8_t *) &virq->vctx[idx];
	
	for (word_t i=0; i < sizeof(vm_context_t); i++)
	    dst[i] = src[i];
    }	

    vm_context_init(&virq->vctx[virq->num_vms]);
    virq->num_vms--;

    if (virq->num_vms == 0)
	virq->current = &dummy_vm;

    return true;
}

static inline void init_root_servers(virq_t *virq)
{
    L4_Word_t dummy;
    L4_Word_t time_control = virq->myself.raw;
    L4_Word_t processor_control = (1 << 16 | virq->mycpu);
    L4_Word_t preemption_control = (3 << 24);
    
    setup_thread_faults(s0);
    setup_thread_faults(roottask);

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
    
    register_system_task(virq->mycpu, roottask, roottask_local, vm_state_runnable, true);
    register_system_task(virq->mycpu, s0, L4_nilthread, vm_state_blocked, true);
    
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
    setup_thread_faults(ptimer);
    
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
    
    
    dprintf(debug_virq, "VIRQ %d migrate tid %t last_balance %d to %d tick %d l4_cpu_cnt %d\n",
	    virq->mycpu, tid, (word_t) virq->current->last_balance, dest_pcpu,
	    (word_t) virq->ticks, l4_cpu_cnt);


    
    unregister_timer_handler(virq->current_idx);
    
    L4_MsgTag_t tag = L4_Receive (tid, L4_ZeroTime);
    if (!L4_IpcFailed(tag))
    {
	printf("VIRQ %d received pending preemption before migration from tid %t\n",  
	       virq->mycpu, tid);
	state = vm_state_runnable;
    }

    vm_context_t *handler = register_timer_handler(vm, vcpu, dest_pcpu, tid, period_len, 
						     state, true, false); 
    ASSERT(handler);
    handler->balance_pending = true;    
    handler->old_pcpu = virq->mycpu;
    handler->last_balance = virqs[dest_pcpu].ticks; // MANUEL: set last_balance to the current "time" of the NEW pcpu
}



static void virq_thread(void *param UNUSED, hthread_t *htread UNUSED)
{
    
    virq_t *virq = &virqs[L4_ProcessorNo()];
    L4_ThreadId_t ptimer = L4_nilthread;
    L4_ThreadId_t pirq = L4_nilthread;
    L4_ThreadId_t current = L4_nilthread;
    L4_ThreadId_t from = L4_nilthread;
    
    L4_ThreadId_t preempter = L4_nilthread;
    L4_ThreadId_t activator = L4_nilthread;
    L4_ThreadId_t spreempter = L4_nilthread;
    L4_ThreadId_t sactivator = L4_nilthread;
    L4_ThreadId_t tspreempter = L4_nilthread;
    L4_ThreadId_t tsactivator = L4_nilthread;
    
    L4_ThreadId_t afrom = L4_nilthread;
    L4_ThreadId_t sfrom = L4_nilthread;

    L4_MsgTag_t tag;
    L4_Word_t hwirq = 0;

    bool do_timer = false, do_hwirq = false;
    bool reschedule = false;


    ASSERT(virq->myself == L4_Myself());
    ASSERT(virq->mycpu == L4_ProcessorNo());

    virq->utcb_mrs = (L4_Msg_t *) __L4_X86_Utcb (); 

    if (virq->mycpu == 0)
	init_root_servers(virq);

    ptimer.global.X.thread_no = ptimer_irqno_start + virq->mycpu;
    ptimer.global.X.version = 1;
    

    current = virq->current->monitor_tid;
    ASSERT(virq->current->system_task);
    
    dprintf(debug_startup, "VIRQ %d started current %t\n", virq->mycpu, current);
    //L4_KDB_Enter("VIRQ INIT");
    
    
    virq->pfreq = L4_ProcDescInternalFreq(L4_ProcDesc(l4_kip, virq->mycpu)) / 1000;
    dprintf(debug_startup, "VIRQ %d processor frequency %d\n", virq->mycpu, virq->pfreq);
	    

    
    tag = L4_Wait(&from);
    
    for (;;)
    {
	console_read();
	
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
	    // System task
	    L4_Word_t lidx = tid_to_vm_idx(virq, from);
	    ASSERT(lidx < MAX_VIRQ_VMS);
	    from = virq->vctx[lidx].monitor_tid;
	}

	switch( L4_Label(tag) )
	{
	case MSG_LABEL_BLOCKED:
	{
	    //	idle/blocking	-> IPC to lcs(current)
	    //	 virtualsender	 = sender's VM tid 
	    //	 actualsender	 = sender	    
	    //   tstate item:
	    //    word0		 = sender's scheduler
	    //    word1		 = preempter = partner
	    //    word3		 = preempter's VM tid
	    //    word5		 = preempter's scheduler
	    ASSERT(from == virq->current->monitor_tid);
	    
	    virq->mr = 3 + (L4_CTRLXFER_GPREGS_SIZE+1);
	    virq->mr += L4_Get (virq->utcb_mrs, virq->mr, &virq->tstate);
	    
	    afrom = L4_IpcPropagated (tag) ? L4_ActualSender() : from;
	    sfrom.raw = virq->tstate.regs.reg[0];
	    preempter.raw   = virq->tstate.regs.reg[1];
	    tspreempter.raw = virq->tstate.regs.reg[3];
	    spreempter.raw  = virq->tstate.regs.reg[5];

	    dprintf(debug_virq+1, "VIRQ %d idle IPC from %t current %t preempter %t num VMs %d\n", 
		    virq->mycpu, afrom, current, preempter, virq->num_vms);

	    // Store messages from subordinate threads
	    if (from != afrom)
		L4_Store(tag, &virq->current->last_msg);


	    virq_earm_update(virq);
	   
	    ASSERT(virq->num_vms);
	    ASSERT(virq->current->state == vm_state_runnable);
	    
	    virq_set_state(virq, virq->current_idx, vm_state_blocked);
	    virq->current->last_tid = afrom;
	    virq->current->last_scheduler = sfrom;
	    
	    L4_Word_t pidx = tid_to_vm_idx(virq, tspreempter);
	    ASSERT(pidx < MAX_VIRQ_VMS);
	    
	    if (virq->vctx[pidx].state == vm_state_runnable)
	    {
		// Perform handoff by switching current
		virq->current_idx = pidx;
		virq->current = &virq->vctx[pidx];
		current = virq->current->last_tid;
	    }
	    else
	    {
		reschedule = true;
	    }
	}
	break;
	case MSG_LABEL_PREEMPTION:
	{
	    //	preemption		-> IPC to lcs(current, preempter)
	    //	 virtualsender	 = sender's VM tid 
	    //	 actualsender	 = sender	    
	    //   tstate item:
	    //    word0		 = sender's scheduler
	    //    word1		 = preempter
	    //    word3		 = preempter's VM tid
	    //    word5		 = preempter's scheduler
            
	    virq->mr = 3 + (L4_CTRLXFER_GPREGS_SIZE+1);
	    virq->mr += L4_Get (virq->utcb_mrs, virq->mr, &virq->tstate);

	    afrom = (L4_IpcPropagated (tag)) ? L4_ActualSender() : from;
	    sfrom.raw = virq->tstate.regs.reg[0];

	    preempter.raw   = virq->tstate.regs.reg[1];
	    tspreempter.raw = virq->tstate.regs.reg[3];
	    spreempter.raw  = virq->tstate.regs.reg[5];
	    
	    if (is_l4_irq_tid(from))
	    {
		if (preempter == current)
		{
		    // current preeempted by an irq
		    ASSERT (virq->current->state == vm_state_runnable);
		    virq->current->last_tid = preempter;
		    virq->current->last_scheduler = spreempter;
		}
		else 
		{
		    // virq preeempted by an irq, reschedule
		    ASSERT(preempter == virq->myself || preempter == idle_tid);
		    preempter = virq->myself;
		}
		
		//swap current and preempter
		preempter = from;
	    }
	    else 
	    {
		ASSERT(from == virq->current->monitor_tid);
		ASSERT (virq->current->state == vm_state_runnable);
		
		virq->current->last_tid = afrom;
		virq->current->last_scheduler = sfrom;
		// Store messages from subordinate threads
		if (from != afrom)
		    L4_Store(tag, &virq->current->last_msg);
		
	    }

	    virq_set_state(virq, virq->current_idx, virq->current->state);

	    virq_earm_update(virq);
            
	    if (preempter == ptimer)
	    {
		dprintf(debug_virq+1, "VIRQ %d current %t preempted by timer\n", 
			virq->mycpu, current);
		
		do_timer = true;
	    }
	    else if (is_l4_irq_tid(preempter))
	    {
		hwirq = preempter.global.X.thread_no;
		dprintf(debug_virq+1, "VIRQ %d current %t preempted by irq %d %p\n", 
			virq->mycpu, current, hwirq, preempter);
		
		do_hwirq = true;
		
	    }
	    else if (preempter != virq->myself)
	    {
		// Send-only / Notification current->preempter
		L4_Word_t fidx = tid_to_vm_idx(virq, tspreempter);
		ASSERT(fidx < MAX_VIRQ_VMS);
		
		dprintf(debug_virq+1, "VIRQ %d current %t preempted by VM %d %p last %t state %C\n", 
			virq->mycpu, current, fidx, preempter, virq->vctx[fidx].last_tid,
			DEBUG_TO_4CHAR(vctx_state_string[virq->vctx[fidx].state]));		

		if (preempter == virq->vctx[fidx].last_tid)
		{
		    //Last tid gets a send-only IPC, just reactivate
		    virq_set_state(virq, fidx, vm_state_runnable);
		}
		else 
		{	
		    //Preempter gets send-only and last_tid was preempted 
		    if (!virq->vctx[fidx].system_task)
		    {
			if (preempter == virq->vctx[fidx].monitor_tid)
			{
			    virq->vctx[fidx].last_tid = virq->vctx[fidx].monitor_tid;
			}
			else
			{
			    virq->vctx[fidx].vm->store_activated_tid(virq->current->vcpu, preempter);
			    virq->vctx[fidx].evt_pending = true;
			}
		    }
		    //if (virq->vctx[fidx].state != vm_state_blocked)
		    virq_set_state(virq, fidx, vm_state_runnable);
		}
	    }
	}
	break;
	case MSG_LABEL_ACTIVATION:
	{
	    //	activation	-> IPC to lcs(current, activator, preempter)
	    //	 virtualsender	 = activatees's VM tid 
	    //	 actualsender	 = activatee	    
	    //   tstate item:
	    //    word0		 = activatee's scheduler	
	    //	  word1		 = preempter
	    //    word2		 = activator
	    //    word3		 = preempter's VM tid
	    //    word4		 = activator's VM tid
	    //    word5		 = preempter's scheduler
	    //    word6		 = activator's scheduler
	    virq->mr = 3 + (L4_CTRLXFER_GPREGS_SIZE+1);
	    virq->mr += L4_Get (virq->utcb_mrs, virq->mr, &virq->tstate);

	    afrom = L4_IpcPropagated (tag) ? L4_ActualSender() : from;
	    sfrom.raw = virq->tstate.regs.reg[0];

	    preempter.raw   = virq->tstate.regs.reg[1];
	    activator.raw   = virq->tstate.regs.reg[2];
	    tspreempter.raw = virq->tstate.regs.reg[3];
	    tsactivator.raw = virq->tstate.regs.reg[4];
	    spreempter.raw  = virq->tstate.regs.reg[5];
	    sactivator.raw  = virq->tstate.regs.reg[6];

	    // Sender
	    L4_Word_t fidx = tid_to_vm_idx(virq, from);
	    ASSERT(fidx < MAX_VIRQ_VMS);
	    
	    dprintf(debug_virq+1, "VIRQ %d activation of VM %d %t (%C) by VM %d %t (%C) preempter %t (%C)\n", 
		    virq->mycpu, fidx, afrom,  DEBUG_TO_4CHAR(vctx_state_string[virq->vctx[fidx].state]),
		    virq->current_idx, activator, DEBUG_TO_4CHAR(vctx_state_string[virq->current->state]),
		    preempter);
	    
	    virq_set_state(virq, fidx, vm_state_runnable);
	    virq->vctx[fidx].last_tid = afrom;
	    virq->vctx[fidx].last_scheduler = sfrom;
	    
	    // Store messages from subordinate threads
	    if (from != afrom)
		L4_Store(tag, &virq->vctx[fidx].last_msg);

	    // Activator == current
	    ASSERT(tsactivator == virq->current->monitor_tid);
 	    ASSERT(virq->current->state == vm_state_runnable);
	    
	    virq_set_state(virq, virq->current_idx, vm_state_blocked);
	    virq->current->last_tid = activator;
	    virq->current->last_scheduler = sactivator;
	    

	    // Preempter	    
	    if (preempter != L4_nilthread)
	    {
		L4_Word_t pidx = tid_to_vm_idx(virq, tspreempter);
		ASSERT(pidx < MAX_VIRQ_VMS);
		
		if (pidx != fidx)
		{
		    ASSERT(virq->vctx[pidx].state == vm_state_blocked);
		    virq_set_state(virq, pidx, vm_state_runnable);
		    virq->vctx[pidx].last_tid = preempter;
		    virq->vctx[pidx].last_scheduler = spreempter;
		}
		else
		{
		    ASSERT(virq->vctx[fidx].last_tid == virq->vctx[fidx].monitor_tid);
		}

	    }
	    
	    virq_earm_update(virq);

	    // Perform handoff by switching current
	    virq->current_idx = fidx;
	    virq->current = &virq->vctx[fidx];
	    current = virq->current->last_tid;;
	}
	break;
	case MSG_LABEL_YIELD:
	{
	    ASSERT(from == virq->current->monitor_tid);
	    
	    virq_set_state(virq, virq->current_idx, vm_state_yield);
	    virq->current->last_tid = from;
	    
	    virq_earm_update(virq);

	    /* yield,  fetch dest */
	    L4_ThreadId_t dest;
	    L4_StoreMR(1, &dest.raw);
	    
	    if (dest != L4_nilthread && dest != virq->current->monitor_tid)
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
		dprintf(debug_virq+1, "VIRQ %d yield IPC current %t dest %t, reschedule\n", virq->mycpu, current, dest);
		reschedule = true;
	    }

	}
	break;
	case MSG_LABEL_IRQ_ACK:
	{
	    ASSERT(from == virq->current->monitor_tid);
	    afrom = L4_IpcPropagated (tag) ? L4_ActualSender() : from;
	    virq->current->last_tid = afrom;

	    L4_StoreMR( 1, &hwirq );
	    ASSERT( hwirq < ptimer_irqno_start );
	    
	    // Store messages from subordinate threads
	    if (from != afrom)
		L4_Store(tag, &virq->current->last_msg);

	    /* Verify that sender belongs to associated VM */
	    if (pirqhandler[hwirq].virq != virq)
	    {
		dprintf(debug_virq, "VIRQ %d IRQ %d remote ack by %t pirq handler %t\n",
			virq->mycpu, hwirq, current, pirqhandler[hwirq].virq->myself);
		
		ASSERT(tid_to_vm_idx(virq, current) < MAX_VIRQ_VMS);
		ASSERT(pirqhandler[hwirq].virq->vctx[pirqhandler[hwirq].idx].vm == 
		       virq->vctx[tid_to_vm_idx(virq, current)].vm);
		L4_Set_VirtualSender(pirqhandler[hwirq].virq->myself);
		L4_Set_MsgTag(packtag);
	    }
	    else
	    {
		dprintf(debug_virq+1, "VIRQ %d IRQ %d ack by  %t pirq handler %t\n",
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
	    printf( "VIRQ %d unexpected msg current %t tag %x label %x\n", 
		    virq->mycpu, current, tag.raw, L4_Label(tag));
	    L4_KDB_Enter("VIRQ BUG");
	}
	break;
	}

	ASSERT(virq->num_vms);
	
	if (do_timer)
	{
	    /* Acknlowledge Timer */
	    L4_Set_MsgTag(acktag);
	    tag = L4_Reply(ptimer);
	    ASSERT(!L4_IpcFailed(tag));		
	    
	    virq->ticks++;
#if defined(LATENCY_BENCHMARK)
	    virq_latency_benchmark(virq);
#endif
#if defined(VIRQ_PFREQ)
	    if (!(virq->ticks % 1000))
		freq_adjust_pstate();
#endif

#if defined(cfg_earm)
	    
	    L4_Word_t ticks = (L4_Word_t) (virq->ticks & 0xFFFFFFFF);
	    if (ticks > (virq->apticks + EARM_VIRQ_TICKS))		
	    {		
		{
		    L4_Word64_t apower = EARM_CPU_DIVISOR * virq->senergy / ((virq->stsc / virq->pfreq) + 1);
		    L4_Word64_t vpower = virq->senergy / ((ticks - virq->apticks) * 10 * tick_ms);
		    
		    virq->vpower = ((EARM_CPU_EXP * vpower) + ((100-EARM_CPU_EXP) * virq->vpower)) / 100;       
		    virq->apower = ((EARM_CPU_EXP * apower) + ((100-EARM_CPU_EXP) * virq->apower)) / 100;       
		}
		
		for (L4_Word_t i = 0; i < virq->num_vms; i++)
		{	
		    L4_Word_t apower = EARM_CPU_DIVISOR * virq->vctx[i].senergy / ((virq->vctx[i].stsc / virq->pfreq) + 1);
		    L4_Word_t vpower = virq->vctx[i].senergy / ((ticks - virq->apticks) * 10 * tick_ms);
		    
		    virq->vctx[i].apower = apower;
		    virq->vctx[i].vpower = vpower;
		    
		    dprintf(debug_earm, "VIRQ %d VM %d  senergy %8d apower %6d vpower %6d\n", 
			    virq->mycpu, i, (L4_Word_t) virq->vctx[i].senergy, virq->vctx[i].apower, 
			    virq->vctx[i].vpower);
			
		    virq->vctx[i].senergy = 0;					
		    virq->vctx[i].stsc = 0;					
		    

		}

		
		dprintf(debug_earm, "VIRQ %d TOTAL senergy %8d apower %6d vpower %6d\n", 
			virq->mycpu, (L4_Word_t) virq->senergy, virq->apower, virq->vpower);
		

		virq->senergy = 0;						
		virq->stsc = 0;						
		virq->apticks = ticks;				

	    }    
	    else if (ticks < virq->apticks) 
	    {
		virq->apticks = ticks;
	    }

	    if (!(virq->ticks % 1000))
		earmmanager_print_resources();
#endif	    


	    if (migrate_vm(virq->current, virq))
	    {
		L4_Word_t dest_pcpu = (virq->mycpu + 1) % l4_cpu_cnt;
		migrate_vcpu(virq, dest_pcpu);
		
		ASSERT (virq->num_vms);
	    }
	    
	    for (L4_Word_t i = 0; i < virq->num_vms; i++)
	    {
		if (virq->vctx[i].system_task)
		    continue;
		
		/* Deliver pending virq interrupts */
		if (virq->ticks - virq->vctx[i].last_tick >= virq->vctx[i].period_len)
		{
		    dprintf(debug_virq+1, "VIRQ %d deliver TIMER IRQ %d to VM %d last_tid %t state %d\n",
			    virq->mycpu, 
			    ptimer_irqno_start + virq->vctx[i].vcpu, i,
			    virq->vctx[i].last_tid,
			    virq->vctx[i].state);
		    
		    virq->vctx[i].last_tick = virq->ticks;
		    virq->vctx[i].vm->set_virq_pending(ptimer_irqno_start + virq->vctx[i].vcpu);
		    virq->vctx[i].evt_pending = true;
		    
		    if( virq->vctx[i].state == vm_state_yield)
			virq_set_state(virq, i, vm_state_runnable);
		}
	    }
	    reschedule = true;
	}

	if (do_hwirq)
	{
	    ASSERT(hwirq < l4_user_base);
	    ASSERT(hwirq < ptimer_irqno_start);
	    
	    word_t hwirq_idx = pirqhandler[hwirq].idx;
	    ASSERT(hwirq_idx < MAX_VIRQ_VMS);
	    
	    virq->vctx[hwirq_idx].vm->set_virq_pending(hwirq);
	    virq->vctx[hwirq_idx].evt_pending = true;

	    /* hwirq; immediately schedule handler VM
	     * TODO: bvt scheduling */	
	    
	    virq_t *hwirq_virq = pirqhandler[hwirq].virq;
	    
	    dprintf(debug_virq+1, "VIRQ %d deliver  IRQ %d to VM %d last_tid %t state %d\n",
		    virq->mycpu, hwirq, hwirq_idx,
		    hwirq_virq->vctx[hwirq_idx].last_tid,
		    hwirq_virq->vctx[hwirq_idx].state);

    
	    if (hwirq_virq == virq && hwirq_virq->vctx[hwirq_idx].state != vm_state_blocked)
	    {
		virq_set_state(virq, hwirq_idx, vm_state_runnable);
		virq->current_idx = hwirq_idx;
	    }
	    reschedule = false;
	}
	
	if (reschedule)	
	{
#if defined(cfg_earm)
	    dprintf(debug_earm, "VIRQ apower %d  vpower %d cpower %d\n", 
		    virq->apower, virq->vpower, virq->cpower);
		
	    L4_Word_t esum = 0;

	    for (L4_Word_t i = 0; i < virq->num_vms; i++)
	    {
		if (virq->vctx[i].state == vm_state_runnable)
		{
		    switch (virq->scheduler)
		    {
		    case 0:
			virq->vctx[i].eticket = 1 + (100 * virq->vctx[i].ticket / ((virq->vctx[i].apower / 1000) + 1));
			break;
		    case 1:
			virq->vctx[i].eticket = 1 + (100 * virq->vctx[i].ticket / ((virq->vctx[i].vpower / 1000) + 1));
			break;
		    case 2:
			virq->vctx[i].eticket = virq->vctx[i].ticket;
			break;	
		    default:
			ASSERT(false);
			break;
		    }
			    
		    dprintf(debug_earm, "VIRQ %d VM %d scheduler %d ticket %03d vpower %06d apower %d -> eticket %03d\n", 
			    virq->mycpu, i, virq->scheduler, virq->vctx[i].ticket, virq->vctx[i].vpower, virq->vctx[i].eticket);
		    esum += virq->vctx[i].eticket;
		}
		    
	    }
	    
	    
	    if (virq->cpower)
	    {
		L4_Word_t cap = virq->cpower * 100 / (virq->apower + 1);
		
		if (cap < 100)
		{
		    L4_Word_t lottery = rand() % 100;
		    
		    if (lottery > cap) 
			esum = 0;
		    dprintf(debug_earm, "VIRQ %d lottery cap %03d %03d\n", virq->mycpu, lottery, cap);
		}
	    }	    
	    
	    if (esum)
	    {
		    
		L4_Word_t lottery = rand() % esum;
		L4_Word_t winner = 0;

		dprintf(debug_earm, "VIRQ %d lottery %03d %03d\n", virq->mycpu, lottery, esum);

		for (L4_Word_t i = 0; i < virq->num_vms; i++)
		    if (virq->vctx[i].state == vm_state_runnable)
		    {
			winner += virq->vctx[i].eticket;
			if (lottery < winner) 
			{
			    dprintf(debug_earm, "VIRQ %d lottery %03d  winner %03d esum %03d current %d eticket %03d\n", 
				    virq->mycpu, lottery, winner, esum, i, virq->vctx[i].eticket);
			    virq->scheduled = i;
			    break;
			}
		    }
	    }
	    else
	    {
		// No runnable thread
		virq->scheduled = 0;
	    }

#else
	    /* Perform RR scheduling */	
	    for (L4_Word_t i = 0; i < virq->num_vms; i++)
	    {
		virq->scheduled = (virq->scheduled + 1) % virq->num_vms;
		if (virq->vctx[virq->scheduled].state == vm_state_runnable)
		    break;
	    }
#endif	    
	    virq->current_idx = virq->scheduled;
	    ASSERT(virq->current_idx < virq->num_vms);
	}
	
	do_timer = do_hwirq = false;

	virq->current = &virq->vctx[virq->current_idx];
	
	if (virq->current->state == vm_state_runnable)
	{
	    current = virq->current->last_tid;
	    ASSERT(current != L4_nilthread);
	    
	    L4_Set_MsgTag(continuetag);

	    if (!virq->current->started)
	    {
		virq->current->started = true;
		
		dprintf(debug_virq, "VIRQ start %t\n", current);
		
		L4_Clear( &virq->current->last_msg );
		L4_Set_VirtualSender(roottask);
		L4_Set_Propagation(&virq->current->last_msg.tag);
		
		if (!virq->current->system_task)
		{
		    L4_GPRegsCtrlXferItem_t start_item;
		    L4_Init(&start_item);
		    L4_Set(&start_item, 
			   L4_CTRLXFER_GPREGS_EIP, virq->current->vm->binary_entry_vaddr );		// ip
		    L4_Set(&start_item, 
			   L4_CTRLXFER_GPREGS_ESP, virq->current->vm->binary_stack_vaddr );		// sp
		    L4_Append(&virq->current->last_msg, &start_item);
		}

		L4_Load(&virq->current->last_msg);

	    }

	    
	    if (virq->current->balance_pending)
	    {
		ASSERT(virq->current->old_pcpu < IResourcemon_max_cpus);
		dprintf(debug_virq, "VIRQ %d propagate to current %t after balance from pcpu %d tid %t\n",
			virq->mycpu, current, virq->current->old_pcpu, virqs[virq->current->old_pcpu].myself);
		
		L4_Set_VirtualSender(virqs[virq->current->old_pcpu].myself);
		tag =  pcontinuetag;
		virq->current->old_pcpu = IResourcemon_max_cpus;
		virq->current->balance_pending = false;
		virq->current->last_balance = virq->ticks;
		L4_KDB_Enter("UNTESTED BALANCE PENDING");
	    }
	    
	    if (current != virq->current->monitor_tid)
	    {
		if (virq->current->evt_pending  && virq->current->last_state != vm_state_blocked) 
		{
		    dprintf(debug_virq+1, "VIRQ %d VM %d pending irq last_msg <%x:%x:%x>\n",
			    virq->mycpu, virq->current_idx, virq->current->last_msg.msg[0], 
			    virq->current->last_msg.msg[1], virq->current->last_msg.msg[2]);
		    
		    //Forward preeemptee's last preemption record
		    virq->current->vm->store_preemption_msg(virq->current->vcpu, current, 
							    &virq->current->last_msg);
		    virq->current->last_msg.msg[2] = virq->current->last_msg.tag.raw;
		    current = virq->current->monitor_tid;
		    L4_Set_MsgTag(continuetag);
		    virq->current->evt_pending = false;

		}
		else	    
		{
		    ASSERT(virq->current->last_scheduler != L4_nilthread);
		    L4_Set_VirtualSender(virq->current->last_scheduler);
		    L4_Set_MsgTag(pcontinuetag);
		}
	    }
	    
	    dprintf(debug_virq, "VIRQ %d dispatch VM %d current %t monitor %t\n",
		    virq->mycpu, virq->current_idx, current, virq->current->monitor_tid);
	    
	}
	else 
	{

	    dprintf(debug_virq+1, "VIRQ no running VM (current %t), switch to idle\n", 
		    virq->mycpu, current);
	     
	    __asm__ __volatile__ ("hlt" ::: "memory" );
	    current = L4_nilthread;
	}
	
	reschedule = false;
	
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
	ASSERT(virqs[pcpu].mycpu == pcpu);
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
    roottask = L4_Myself();
    roottask_local = L4_MyLocalId();
    s0 = L4_GlobalId (L4_ThreadIdUserBase(l4_kip), 1);
    l4_cpu_cnt = l4_cpu_cnt;
    ptimer_irqno_start = L4_ThreadIdSystemBase(l4_kip) - l4_cpu_cnt;
    
    /* PIC timer = 2ms, APIC = 1ms */
    tick_ms = (ptimer_irqno_start == 16) ? 2 : 1;
    period_len = 10 / tick_ms;
    dprintf(debug_virq, "VTimer ms/ticks %d period len %d\n", period_len);

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
    powernow_init(min(IResourcemon_max_cpus,L4_NumProcessors(l4_kip))); // MANUEL
#endif

    vm_context_init(&dummy_vm);
    
    for (L4_Word_t cpu=0; cpu < min(IResourcemon_max_cpus,L4_NumProcessors(l4_kip)); cpu++)
    {
	virq_t *virq = &virqs[cpu];
	
	for (L4_Word_t v_idx=0; v_idx < MAX_VIRQ_VMS; v_idx++)
	    vm_context_init(&virq->vctx[v_idx]);
    
	virq->ticks = 0;
	virq->num_vms = 0;
	virq->current = &dummy_vm;
	virq->scheduled = 0;
	
	for (L4_Word_t i=0; i < 8; i++)
	    virq->pmcstate[i] = 0;
	
	for (L4_Word_t irq=0; irq < MAX_IRQS; irq++)
	{
	    pirqhandler[irq].idx = MAX_VIRQ_VMS;
	    pirqhandler[irq].virq = NULL;
	}
	virq->thread = NULL;
	virq->thread = get_hthread_manager()->create_thread( 
	    (hthread_idx_e) (hthread_idx_virq + cpu), PRIO_VIRQ, l4_smallspaces_enabled, virq_thread);

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

	if (cpu == 0)
	{
	    setup_thread_faults(s0);
	    setup_thread_faults(roottask);
	    
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

	virq->myself = virq->thread->get_global_tid();
	virq->mycpu = cpu;
	
	L4_ThreadId_t ptimer = L4_nilthread;
	ptimer.global.X.thread_no = ptimer_irqno_start + virq->mycpu;
	ptimer.global.X.version = 1;
	L4_Start( virq->thread->get_global_tid() ); 
	associate_ptimer(ptimer, virq);

	
    }
    
}
    


//0 VM 5  senergy     4616 apower   4438 vpower     10
//139646 0 0001 u  154 000d8002 0802   0001   0001   VIRQ 0 TOTAL senergy        0 apower   3494 vpower   3577
//139648 0 0001 u  154 000d8002 2340   0005   0004   VIRQ apower 3494  vpower 3577 cpower 2500                
//139651 0 0001 u  154 000d8002 0892   0001   0001   VIRQ 0 lottery cap 005 071      
