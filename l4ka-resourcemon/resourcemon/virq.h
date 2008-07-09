/*********************************************************************
 *                
 * Copyright (C) 2006-2008,  Karlsruhe University
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

typedef struct { 
    vm_t		*vm;
    L4_Word_t		domain;
    L4_Word_t		vcpu;
    vm_state_e		state;
    L4_Word_t		period_len;
    L4_Word64_t		last_tick;
    L4_Word64_t		last_balance;
    L4_Word_t		old_pcpu;
    L4_ThreadId_t	monitor_tid;	// Monitor TID
    L4_ThreadId_t	last_tid;	// Last preempted TID of that VM
    L4_ThreadId_t	last_scheduler;	// Last scheduler of last_tid
    L4_Msg_t		last_msg;	// Message contents of last preemption VM
    L4_IA32_PMCCtrlXferItem_t last_pmc;
    bool		irq_pending;	
    bool		balance_pending;
    bool		started;
    bool		system_task;
} vm_context_t;


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


typedef struct 
{
    virq_t *virq;
    word_t idx;
} pirqhandler_t;

extern virq_t virqs[IResourcemon_max_cpus];
extern L4_Word_t ptimer_irqno_start, ptimer_irqno_end;

INLINE void setup_thread_faults(L4_ThreadId_t tid, bool irq=false) 
{
    /* Turn on ctrlxfer items */
    L4_Msg_t ctrlxfer_msg;
    L4_Word64_t fault_id_mask = (1<<2) | (1<<3) | (1<<5);
    L4_Word_t fault_mask = 
	L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_TSTATE_ID) |
#if defined(cfg_eacc)
	L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_PMCREGS_ID) |
#endif
	(irq ? 0 : L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_GPREGS_ID));	
    
    
    L4_Clear(&ctrlxfer_msg);
    L4_AppendFaultConfCtrlXferItems(&ctrlxfer_msg, fault_id_mask, fault_mask, false);
    L4_Load(&ctrlxfer_msg);
    L4_ConfCtrlXferItems(tid);

}

#endif /* !__HOME__STOESS__RESOURCEMON__RESOURCEMON__VIRQ_H__ */
