/*********************************************************************
 *                
 * Copyright (C) 2006,  Karlsruhe University
 *                
 * File path:     vtime.cc
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
#include <common/debug.h>
#include <common/hthread.h>
#include <common/console.h>
#include <resourcemon/resourcemon.h>
#include <resourcemon/vm.h>

#if defined(cfg_l4ka_vmextensions)

#define VTIMER_PERIOD_LEN		10000
#define MAX_VTIMER_VM			10

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
    } handler[MAX_VTIMER_VM];
    
    L4_Word_t	  current_handler;
    L4_Word_t	  num_handlers;
    L4_Word64_t	  period_len;
    L4_Time_t	  period;
    hthread_t	  *thread;
} vtime_t;

vtime_t vtimers[IResourcemon_max_cpus];


#define MSG_LABEL_PREEMPTION	0xffd0
#define MSG_LABEL_YIELD		0xffd1
#define MSG_LABEL_CONTINUE	0xffd2
#define MSG_LABEL_IRQ		0xfff0

const L4_MsgTag_t hwirqtag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_IRQ } };
const L4_MsgTag_t continuetag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_CONTINUE} }; 

static void vtimer_thread( 
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
    hout << "Vtimer TID: " << L4_Myself() << "\n"; 

    L4_Word_t cpu = L4_ProcessorNo();
    vtime_t *vtimer = &vtimers[cpu];
    
    L4_Sleep( vtimer->period );
    L4_ThreadId_t myself = L4_Myself();
    L4_Word_t     current = vtimer->current_handler;
    L4_ThreadId_t to = vtimer->handler[current].tid;
    L4_ThreadId_t from = vtimer->handler[current].tid;
    L4_ThreadId_t reply;
    L4_MsgTag_t tag;
    L4_Word_t timeouts = L4_Timeouts(L4_ZeroTime, vtimer->period);
    
    L4_Set_MsgTag(hwirqtag);
   
    for (;;)
    {
	L4_Clock_t schedule_time = L4_SystemClock();
	L4_Set_TimesliceReceiver(from);
	tag = L4_Ipc( to, from, timeouts, &reply);
						      
	timeouts = L4_Timeouts(L4_ZeroTime, vtimer->period);
	
	if (!L4_IpcFailed(tag))
	{
	    /* Got a message, so irq thread preempted before timeout*/
	    if (L4_Label(tag) == MSG_LABEL_YIELD)
	    {
		/* 
		 * yield, so fetch dest
		 * actually we should verify that it's an IRQ thread
		 */
		L4_Word64_t progress = L4_SystemClock().raw - schedule_time.raw;
		if (progress < vtimer->period_len)
		{
		    L4_ThreadId_t dest;
		    L4_StoreMR(13, &dest.raw);
		    vtimer->handler[current].state = vm_state_idle;
		    
		    if (dest == L4_nilthread || dest == from)
		    {
			to = L4_nilthread;
			for (word_t idx=0; idx < vtimer->num_handlers; idx++)
			    if (vtimer->handler[idx].state != vm_state_idle)
			    {
				current = idx;
				to = from = vtimer->handler[current].tid;
			    }
			
		    }
		    else 
			to = from = dest;
		    
		    L4_Set_MsgTag(continuetag);
		    timeouts = L4_Timeouts(L4_ZeroTime, L4_TimePeriod(vtimer->period_len - progress));
		    continue;
		}
	    }
	    else if (L4_Label(tag) == MSG_LABEL_PREEMPTION)
	    {
		/* 
		 * was preempted by us, so restart message
		 */
		to = from;
		continue;		

	    }
	    else 
	    {
		hout << "IRQ strange IPC"
		     << " current handler " << vtimer->handler[current].tid
		     << " label " << (void *) L4_Label(tag) << "\n"; 
		L4_KDB_Enter("Vtimer BUG");
	    }
	}
	else if( (L4_ErrorCode() & 0xf) == 2 ) 
	{
	    /* 
	     * send timeout: the handler is running or polling (for us)
	     */
	    to = L4_nilthread;
	    continue;
	}
	/*
	 * receive timeout: we sent the irq but didn't receive a preemption 
	 */
	
	if (++vtimer->current_handler == vtimer->num_handlers)
	    vtimer->current_handler = 0;
	
	current = vtimer->current_handler;
	vtimer->handler[current].state = vm_state_running;
	to = from = vtimer->handler[current].tid;
	vtimer->handler[current].vm->set_vtimer_irq_pending(cpu, vtimer->handler[current].idx);
	L4_Set_MsgTag(hwirqtag);
	
	//if (num_vtime_handlers > 1)
	//  hout << "->" << vtime_handler[current_handler_idx] << "\n";

    }


}

bool associate_virtual_timer_interrupt(vm_t *vm, const L4_ThreadId_t handler_tid, L4_Word_t cpu)
{
    /*
     * We install a timer thread that ticks with frequency 
     * 10ms / num_vtime_handlers
     */
    vtime_t *vtimer = &vtimers[cpu];
    
    if (vtimer->num_handlers == 0)
    {
	vtimer->thread = get_hthread_manager()->create_thread( 
	    (hthread_idx_e) (hthread_idx_vtimer + cpu), PRIO_IRQ,
	    vtimer_thread);
	
	if( !vtimer->thread )
	{	
	    hout << "Could not install vtimer TID: " 
		 << vtimer->thread->get_global_tid() << '\n';
    
	    return false;
	} 
	L4_KernelInterfacePage_t * kip = (L4_KernelInterfacePage_t *)
	    L4_GetKernelInterface ();
	L4_ThreadId_t s0 = L4_GlobalId (kip->ThreadInfo.X.UserBase, 1);

	if( !L4_Set_Priority(s0, PRIO_ROOTSERVER) )
	{
	    hout << "Error: unable to set SIGMA0's  priority to " << PRIO_ROOTSERVER
		 << ", L4 error code: " << L4_ErrorCode() << '\n';
	    return false;
	}
	if( !L4_Set_Priority(L4_Myself(), PRIO_ROOTSERVER) )
	{
	    hout << "Error: unable to set SIGMA0's  priority to " << PRIO_ROOTSERVER
		 << ", L4 error code: " << L4_ErrorCode() << '\n';
	    return false;
	}
	
	if (!L4_Set_ProcessorNo(vtimer->thread->get_global_tid(), cpu))
	{
	    hout << "Error: unable to set vtimer threads cpu to " << cpu
		 << ", L4 error code: " << L4_ErrorCode() << '\n';
	    return false;
	}

    }
    
    /*
     * We install a timer thread that ticks with frequency 
     * 10ms / num_registered_handlers
     */
    
    L4_Word_t handler_idx = 0;
    for (L4_Word_t h_idx=0; h_idx < vtimer->num_handlers; h_idx++)
    {
	if (vtimer->handler[h_idx].vm == vm)
	    handler_idx++;
	if (vtimer->handler[h_idx].tid == handler_tid)
	{
	    hout << "Vtime handler"
		 << " TID " << handler_tid
		 << " already registered"
		 << "\n";
	    return true;
	}
    }	

    if (vtimer->num_handlers == MAX_VTIMER_VM)
    {
	hout << "Vtimer reach maximum number of handlers"
	     << " (" << vtimer->num_handlers << ")"
	     << "\n"; 
	return false;
    }
    
    vtimer->handler[vtimer->num_handlers].vm = vm;
    vtimer->handler[vtimer->num_handlers].state = vm_state_running;
    vtimer->handler[vtimer->num_handlers].tid = handler_tid;
    vtimer->handler[vtimer->num_handlers].idx = handler_idx;
    vtimer->num_handlers++;
    
    vtimer->period_len = VTIMER_PERIOD_LEN / vtimer->num_handlers;
    vtimer->period = L4_TimePeriod( vtimer->period_len );
    
    L4_Word_t dummy, errcode;

    errcode = L4_ThreadControl( handler_tid, handler_tid, vtimer->thread->get_global_tid(), 
				L4_nilthread, (void *) -1UL );

    if (!errcode)
    {
	hout << "Vtimer error: unable to set handler thread's scheduler "
	     << "L4 error " << errcode 
	     << "\n";
	L4_KDB_Enter("VTimer BUG");
    }

    if (!L4_Schedule(handler_tid, (L4_Never.raw << 16) | L4_Never.raw, 
		     ~0UL, ~0UL, L4_PREEMPTION_CONTROL_MSG, &dummy))
    {
	hout << "Vtimer error: failed to set scheduling parameters for irq thread\n";
	L4_KDB_Enter("VTimer BUG");
    }

    vm->set_vtimer_tid(cpu, vtimer->thread->get_global_tid());

    hout << "Vtimer registered handler " <<  handler_tid
	 << " vtimer_tid " <<  vtimer->thread->get_global_tid()
	 << " cpu " <<  (L4_Word_t) cpu
	 << " period " <<  (L4_Word_t) vtimer->period_len
	 << "\n"; 
    
    vtimer->thread->start();


    return true;
}


bool deassociate_virtual_timer_interrupt(vm_t *vm, const L4_ThreadId_t caller_tid, L4_Word_t cpu)
{
    hout << "Vtimer unregistering handler unimplemented"
	 << " caller" << caller_tid
	 << "\n"; 
    L4_KDB_Enter("Resourcemon BUG");
    return false;
}

void vtimer_init()
{
    for (L4_Word_t cpu=0; cpu < IResourcemon_max_cpus; cpu++)
    {
	for (L4_Word_t h_idx=0; h_idx < MAX_VTIMER_VM; h_idx++)
	{
	    vtimers[cpu].handler[h_idx].tid = L4_nilthread;
	    vtimers[cpu].handler[h_idx].idx = 0;
	}
	vtimers[cpu].num_handlers = 0;
	vtimers[cpu].current_handler = 0;
	vtimers[cpu].period_len = VTIMER_PERIOD_LEN;
	vtimers[cpu].period = L4_TimePeriod( vtimers[cpu].period_len );
    }

}

#endif /* defined(cfg_l4ka_vmextensions) */
