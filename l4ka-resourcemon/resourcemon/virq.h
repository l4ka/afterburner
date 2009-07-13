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
#ifndef __RESOURCEMON__VIRQ_H__
#define __RESOURCEMON__VIRQ_H__

#include <l4/thread.h>
#include <common/hthread.h>
#include <resourcemon/vm.h>
#include <resourcemon/logging.h>
#include <l4/ia32/arch.h>


extern L4_Word_t user_base;
extern L4_ThreadId_t roottask, roottask_local, s0;

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
    L4_Word_t		domain;
    L4_Word_t		vcpu;
    vm_state_e		state;
    L4_Word_t		period_len;
    L4_Word_t		old_pcpu;
    L4_ThreadId_t	monitor_tid;	// Monitor TID
    
    L4_Word64_t		last_tick;
    vm_state_e		last_state;
    L4_Word64_t		last_balance;
    L4_ThreadId_t	last_tid;	// Last preempted TID of that VM
    L4_ThreadId_t	last_scheduler;	// Last scheduler of last_tid
    L4_Msg_t		last_msg;	// Message contents of last preemption VM
    L4_IA32_PMCCtrlXferItem_t last_pmc;
    
    bool		evt_pending;	// irq or send-only message pending
    bool		balance_pending;
    bool		started;
    bool		system_task;
} vm_context_t;


INLINE void vm_context_init(vm_context_t *vm)
{
    vm->vm = NULL;
    vm->domain = L4_LOG_NULL_DOMAIN;
    vm->last_state = vm->state;
    vm->vcpu = IResourcemon_max_vcpus;
    vm->state = vm_state_invalid;
    vm->period_len = 0;
    vm->old_pcpu = IResourcemon_max_vcpus;
    vm->monitor_tid = L4_nilthread;
    
    vm->last_tid = L4_nilthread;
    vm->last_state = vm_state_invalid;
    vm->last_balance = 0;
    vm->last_tid = L4_nilthread;
    vm->last_scheduler = L4_nilthread;
    
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

INLINE bool is_irq_tid(L4_ThreadId_t tid)
{
    ASSERT(user_base);
    return (tid.global.X.thread_no < user_base);
    
}

INLINE bool is_system_task(L4_ThreadId_t tid)
{
    ASSERT(user_base);
    ASSERT(L4_IsGlobalId(tid));
    return (tid != L4_nilthread && 
	    (is_irq_tid(tid) || tid == s0 || tid == roottask));
}

INLINE L4_Word_t tid_to_vm_idx(virq_t *virq, L4_ThreadId_t tid)
{
    ASSERT(L4_IsGlobalId(tid));
    for (word_t idx=0; idx < virq->num_vms; idx++)
    {
	if (virq->vctx[idx].monitor_tid == tid)
	    return idx;
    }
    return MAX_VIRQ_VMS;
		
}


#endif /* !__HOME__STOESS__RESOURCEMON__RESOURCEMON__VIRQ_H__ */
