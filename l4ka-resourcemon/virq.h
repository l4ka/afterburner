/*********************************************************************
 *                
 * Copyright (C) 2006-2009,  Karlsruhe University
 *                
 * File path:     virq.h
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
#if !defined(__L4_IA32_ARCH_VM)
#include <ia32/l4archvm.h>
#endif

class hthread_t;
class vm_t;

bool associate_virtual_interrupt(vm_t *vm, const L4_ThreadId_t irq_tid, const L4_ThreadId_t handler_tid, 
				 const L4_Word_t irq_cpu);
bool deassociate_virtual_interrupt(vm_t *vm, const L4_ThreadId_t irq_tid, const L4_ThreadId_t caller_tid);

#define VIRQ_PERIOD_LEN		10000
#define MAX_VIRQ_VMS		16
#define PRIO_VIRQ		(254)

enum vm_state_e { 
    vm_state_runnable,
    vm_state_yield,
    vm_state_blocked,
    vm_state_invalid
};

extern const char *vm_state_string[4];
    
typedef struct { 
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
    L4_IA32_PMCCtrlXferItem_t last_pmc;
    
    L4_Word_t          ticket;
    
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

    vm->ticket = 10;

    for (L4_Word_t i=0; i < __L4_NUM_MRS; i++)
	vm->last_msg.raw[i] = 0;

    for (L4_Word_t i=0; i < L4_CTRLXFER_PMCREGS_SIZE; i++)
	vm->last_pmc.raw[i] = 0;
    
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
} virq_t;

INLINE void virq_set_state(virq_t *virq, L4_Word_t idx, vm_state_e state)
{
    vm_context_t *vctx = &virq->vctx[idx];
    
    dprintf(debug_virq, "VIRQ set VM %d %t state %C -> %C, was %C",
	    idx, vctx->monitor_tid, 
	    DEBUG_TO_4CHAR(vm_state_string[vctx->state]),
	    DEBUG_TO_4CHAR(vm_state_string[state]),
	    DEBUG_TO_4CHAR(vm_state_string[vctx->last_state]));
    
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

#if defined(cfg_earm)
#define THREAD_FAULT_MASK					\
    L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_TSTATE_ID) |		\
    L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_PMCREGS_ID) |		\
    L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_GPREGS_ID)
#else
#define THREAD_FAULT_MASK					\
    L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_TSTATE_ID) |		\
    L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_GPREGS_ID)
#endif

INLINE void setup_thread_faults(L4_ThreadId_t tid) 
{
    /* Turn on ctrlxfer items */
    L4_Msg_t ctrlxfer_msg;
    L4_Word64_t fault_id_mask = (1<<2) | (1<<3) | (1<<5);
    L4_Word_t fault_mask = THREAD_FAULT_MASK;
    
    L4_Clear(&ctrlxfer_msg);
    L4_AppendFaultConfCtrlXferItems(&ctrlxfer_msg, fault_id_mask, fault_mask, false);
    L4_Load(&ctrlxfer_msg);
    L4_ConfCtrlXferItems(tid);

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
	L4_Word_t errcode = L4_ErrorCode();
	
	printf("VIRQ failed to set scheduling parameters of system task %t error %d\n", tid, errcode);
	
	return NULL;
    }

    vm_context_t *handler = &virq->vctx[virq->num_vms];
    vm_context_init(&virq->vctx[virq->num_vms]);
    
    handler->vm = NULL;
    handler->logid = L4_LOG_ROOTSERVER_LOGID;
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


extern void virq_init();


#endif /* !__VIRQ_H__ */
