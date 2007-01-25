/*********************************************************************
 *                
 * Copyright (C) 2006-2007,  Karlsruhe University
 *                
 * File path:     vtime.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __RESOURCEMON__VTIME_H__
#define __RESOURCEMON__VTIME_H__

#include <l4/thread.h>
#include <common/hthread.h>
#include <resourcemon/vm.h>

bool associate_virtual_timer_interrupt(vm_t *vm, const L4_ThreadId_t handler_tid, L4_Word_t cpu);
bool deassociate_virtual_timer_interrupt(vm_t *vm, const L4_ThreadId_t caller_tid, L4_Word_t cpu);

#define VTIMER_PERIOD_LEN		10000
#define MAX_VTIMER_VM			10
#define PRIO_VTIMER			(253)

const bool debug_vtimer = 0;


enum vm_state_e { 
    vm_state_running, 
    vm_state_idle
};

typedef struct {
    struct { 
	vm_t		*vm;
	L4_ThreadId_t	tid;
	L4_Word_t	idx;
	vm_state_e	state;
	L4_Word_t	period_len;
	L4_Word64_t	last_tick;
    } handler[MAX_VTIMER_VM];
    
    L4_Word_t	  current_handler;
    L4_Word_t	  num_handlers;
    L4_Word64_t	  ticks; 
    hthread_t	  *thread;
} vtime_t;

extern vtime_t vtimers[IResourcemon_max_cpus];

#endif /* !__HOME__STOESS__RESOURCEMON__RESOURCEMON__VTIME_H__ */
