/*********************************************************************
 *                
 * Copyright (C) 2006-2010,  Karlsruhe University
 *                
 * File path:     schedule.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __VIRQ_H__
#define __VIRQ_H__

#include <l4/thread.h>
#include <l4/ia32/arch.h>

class hthread_t;
class vm_t;

extern L4_Word_t schedule_associate_irq(vm_t *vm, const L4_ThreadId_t irq_tid, const L4_ThreadId_t handler_tid, 
                                        const L4_Word_t irq_cpu, const L4_Word_t irq_prio, L4_Word_t *err);
extern L4_Word_t schedule_deassociate_irq(vm_t *vm, const L4_ThreadId_t irq_tid, const L4_ThreadId_t caller_tid,
                                          L4_Word_t *err);
extern void schedule_set_pbudget(L4_Word_t guid, L4_Word_t logid, L4_Word_t budget);
extern void schedule_set_scheduler(L4_Word_t logid);
extern void schedule_setup_thread_faults(L4_ThreadId_t tid);
extern void schedule_init();

L4_INLINE L4_Word_t L4_Schedule_Extended(L4_ThreadId_t dest,
                                         L4_ThreadId_t scheduler,
                                         L4_ThreadId_t pager,
                                         L4_Word_t TimeControl,
                                         L4_Word_t ProcessorControl,
                                         L4_Word_t PrioControl,
                                         L4_Word_t PreemptionControl,
                                         L4_Word_t * old_TimeControl)
{
    L4_Word_t ret = L4_Schedule(dest, TimeControl, ProcessorControl, PrioControl, PreemptionControl, old_TimeControl);
    
    if (!ret && pager != L4_nilthread)
    {
	if (!L4_ThreadControl(dest, dest, L4_Myself(), L4_Myself(), (void *) ~0UL))
	    return 0;

	ret = L4_Schedule(dest, TimeControl, ProcessorControl, PrioControl, PreemptionControl, 
			  old_TimeControl);
	if (!ret) 
	    return 0;
	
	if (!L4_ThreadControl(dest, dest, scheduler, pager, (void *) ~0UL))
	    return 0;
    }
    return ret;
}

#if defined(L4_CTRLXFER_TSTATE_ID)

#define VIRQ_PERIOD_LEN		10000
#define MAX_VIRQ_VMS		16
#define PRIO_VIRQ		(254)


enum vm_state_e { 
    vm_state_runnable,
    vm_state_yield,
    vm_state_blocked,
    vm_state_invalid
};

extern const char *vctx_state_string[4];
extern const char *virq_scheduler_string[3];
    
typedef struct 
{ 
    vm_t		*vm;
    L4_Word_t		logid;
    L4_Word_t		vcpu;
    vm_state_e		state;
    L4_Word_t		period_len;
    L4_Word_t		old_pcpu;
    L4_ThreadId_t	monitor_tid;	   // Monitor TID
    L4_ThreadId_t	monitor_ltid;      // local TID (system task)
    
    L4_Word64_t		last_tick;
    vm_state_e		last_state;
    L4_Word64_t		last_balance;
    L4_ThreadId_t	last_tid;	// Last preempted TID of that VM
    L4_ThreadId_t	last_scheduler;	// Last scheduler of last_tid
    L4_Msg_t		last_msg;	// Message contents of last preemption VM
    
    L4_Word_t          ticket;           
    L4_Word_t          eticket;           
    L4_Word64_t        senergy;
    L4_Word64_t        stsc;
    L4_Word_t          apower;
    L4_Word_t          vpower;
    
    bool		evt_pending;	// irq or send-only message pending
    bool		balance_pending;
    bool		started;
    bool		system_task;
} vm_context_t;


INLINE void vm_context_init(vm_context_t *vm)
{
    vm->vm = NULL;
    vm->logid = L4_LOG_NULL_LOGID;
    vm->last_state = vm->state;
    vm->vcpu = IResourcemon_max_vcpus;
    vm->state = vm_state_invalid;
    vm->period_len = 0;
    vm->old_pcpu = IResourcemon_max_vcpus;
    vm->monitor_tid = L4_nilthread;
    vm->monitor_ltid = L4_nilthread;
    
    vm->last_tid = L4_nilthread;
    vm->last_state = vm_state_invalid;
    vm->last_balance = 0;
    vm->last_tid = L4_nilthread;
    vm->last_scheduler = L4_nilthread;
 
    vm->ticket = 1;
    vm->eticket = 1;
    vm->senergy = 0;
    vm->apower = 0;
    
    for (L4_Word_t i=0; i < __L4_NUM_MRS; i++)
	vm->last_msg.raw[i] = 0;

    vm->evt_pending = false;
    vm->balance_pending = false;
    vm->started = false;
    vm->system_task = false;
}

typedef struct {
    vm_context_t   vctx[MAX_VIRQ_VMS];    
    L4_Word_t	   current_idx;
    vm_context_t   *current;
    L4_Word_t	   scheduled;
    L4_Word_t	   num_vms;
    L4_Word64_t	   ticks; 
    hthread_t	   *thread;
    L4_ThreadId_t  myself;
    L4_Word_t	   mycpu;

    L4_Msg_t      *utcb_mrs;
    L4_Word_t mr;

    L4_TStateCtrlXferItem_t tstate;
    
    L4_Word64_t    pmcstate[8];
    L4_Word_t      pfreq;
    L4_Word64_t    senergy;
    L4_Word64_t    stsc;
    L4_Word_t      apower; 
    L4_Word_t      vpower;
    L4_Word_t      cpower;
    L4_Word_t      apticks;
    L4_Word_t      scheduler; // 0 = EAS/apower, 1 = EAS/vpower, 2 = TIME
    
} virq_t;

INLINE void virq_set_state(virq_t *virq, L4_Word_t idx, vm_state_e state)
{
    vm_context_t *vctx = &virq->vctx[idx];
    
    dprintf(debug_virq, "VIRQ %d set VM %d %t state %C -> %C, was %C",
	    virq->mycpu,
	    idx, vctx->monitor_tid, 
	    DEBUG_TO_4CHAR(vctx_state_string[vctx->state]),
	    DEBUG_TO_4CHAR(vctx_state_string[state]),
	    DEBUG_TO_4CHAR(vctx_state_string[vctx->last_state]));
    
    vctx->last_state = vctx->state;
    vctx->state = state;
}

typedef struct 
{
    virq_t *virq;
    word_t idx;
} pirqhandler_t;

extern virq_t virqs[IResourcemon_max_cpus];
extern L4_Word_t ptimer_irqno_start, ptimer_irqno_end;


INLINE virq_t *get_virq(L4_Word_t pcpu = L4_ProcessorNo())
{
    virq_t *virq = &virqs[pcpu];
    ASSERT(virq->mycpu == pcpu);
    return virq;
}

INLINE vm_context_t *register_system_task(word_t pcpu, L4_ThreadId_t tid, L4_ThreadId_t ltid, vm_state_e state, bool started)
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
        printf("VIRQ failed to set scheduling parameters of system task %t error %d\n", tid, L4_ErrorCode());
        return NULL;
    }

    vm_context_t *handler = &virq->vctx[virq->num_vms];
    vm_context_init(&virq->vctx[virq->num_vms]);
    
    handler->vm = NULL;
    handler->logid = 0;
    handler->vcpu = pcpu;
    handler->state = state;
    handler->period_len = 0;
    handler->started = started;
    handler->system_task = true;
    handler->monitor_tid = tid;
    handler->monitor_ltid = ltid;
    handler->last_tid = tid;

    virq->num_vms++;
	
    dprintf(debug_virq, "VIRQ %d register system vctx %d handler %t / %t\n",
	    virq->mycpu, virq->num_vms - 1, handler->monitor_tid, handler->monitor_ltid);
    
    return handler;
}

INLINE L4_Word_t tid_to_vm_idx(virq_t *virq, L4_ThreadId_t tid)
{
    if (L4_IsGlobalId(tid))
    {
	for (word_t idx=0; idx < virq->num_vms; idx++)
	{
	    if (virq->vctx[idx].monitor_tid == tid)
		return idx;
	}
	return MAX_VIRQ_VMS;
	
    }
    else
    {    
	for (word_t idx=0; idx < virq->num_vms; idx++)
	{
	    if (virq->vctx[idx].monitor_ltid == tid)
		return idx;
	}
	return MAX_VIRQ_VMS;
    }
}



#endif /* defined(L4_CTRLXFER_TSTATE_ID) */

#endif /* !__VIRQ_H__ */
