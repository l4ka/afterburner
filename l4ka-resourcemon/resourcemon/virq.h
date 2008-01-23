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

bool associate_virtual_interrupt(vm_t *vm, const L4_ThreadId_t irq_tid, const L4_ThreadId_t handler_tid);
bool deassociate_virtual_interrupt(vm_t *vm, const L4_ThreadId_t irq_tid, const L4_ThreadId_t caller_tid);

#define VIRQ_PERIOD_LEN		10000
#define MAX_VIRQ_HANDLERS       10
#define PRIO_VIRQ		(254)

enum vm_state_e { 
    vm_state_running, 
    vm_state_preempted,
    vm_state_yield,
    vm_state_blocked,
    vm_state_invalid
};

typedef struct { 
    vm_t	*vm;
    L4_Word_t	vcpu;
    vm_state_e	state;
    L4_Word_t	period_len;
    L4_Word64_t	last_tick;
    L4_Word64_t last_balance;
    L4_Word_t	old_pcpu;
    bool	balance_pending;
    bool	activated;
} virq_handler_t;


typedef struct {
    virq_handler_t handler[MAX_VIRQ_HANDLERS];    
    L4_Word_t	   current_idx;
    virq_handler_t *current;
    L4_Word_t	   scheduled;
    L4_Word_t	   num_handlers;
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

#endif /* !__HOME__STOESS__RESOURCEMON__RESOURCEMON__VIRQ_H__ */
