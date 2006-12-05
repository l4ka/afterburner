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
#define PRIO_VTIMER			(255)
#define PRIO_ROOTSERVER			(254)

enum vm_state_e { 
    vm_state_running, 
    vm_state_idle
};

typedef struct {
    vm_t *vm;
    L4_ThreadId_t tid;
    vm_state_e state;
} vtime_handler_t;

vtime_handler_t vtime_handler[MAX_VTIMER_VM];

L4_Word_t     num_handlers;
L4_Word_t     vtime_current_handler;
L4_Word64_t   vtimer_period_len;
L4_Time_t vtimer_period;
hthread_t *vtimer_thread;

#define MSG_LABEL_PREEMPTION	0xffd0
#define MSG_LABEL_YIELD		0xffd1
#define MSG_LABEL_CONTINUE	0xffd2
#define MSG_LABEL_IRQ		0xfff0

const L4_MsgTag_t hwirqtag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_IRQ } };
const L4_MsgTag_t continuetag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_CONTINUE} }; 

static void vtimer( 
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
    hout << "Vtimer TID: " << L4_Myself() << "\n"; 

    L4_Sleep( vtimer_period );
    L4_Word_t cpu = L4_ProcessorNo();
    L4_ThreadId_t myself = L4_Myself();
    L4_Word_t     current = vtime_current_handler;
    L4_ThreadId_t to = vtime_handler[current].tid;
    L4_ThreadId_t from = vtime_handler[current].tid;
    L4_ThreadId_t reply;
    L4_MsgTag_t tag;
    L4_Word_t timeouts = L4_Timeouts(L4_ZeroTime, vtimer_period);
    
    L4_Set_MsgTag(hwirqtag);
   
    for (;;)
    {
	L4_Clock_t schedule_time = L4_SystemClock();
	L4_Set_TimesliceReceiver(from);
	tag = L4_Ipc( to, from, timeouts, &reply);
						      
	timeouts = L4_Timeouts(L4_ZeroTime, vtimer_period);
	
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
		if (progress < vtimer_period_len)
		{
		    L4_ThreadId_t dest;
		    L4_StoreMR(13, &dest.raw);
		    vtime_handler[current].state = vm_state_idle;
		    
		    if (dest == L4_nilthread || dest == from)
		    {
			to = L4_nilthread;
			L4_Set_MsgTag(continuetag);
			for (word_t idx=0; idx < num_handlers; idx++)
			    if (vtime_handler[idx].state != vm_state_idle)
			    {
				current = idx;
				to = from = vtime_handler[current].tid;
			    }
			    
		    }
		    else 
			to = from = dest;
		    
		    timeouts = L4_Timeouts(L4_ZeroTime, L4_TimePeriod(vtimer_period_len - progress));
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
		     << " current handler " << vtime_handler[current].tid
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
	
	if (++vtime_current_handler == num_handlers)
	    vtime_current_handler = 0;
	
	current = vtime_current_handler;
	vtime_handler[current].state = vm_state_running;
	to = from = vtime_handler[current].tid;
	vtime_handler[current].vm->set_vtimer_irq_pending(cpu);
	L4_Set_MsgTag(hwirqtag);
	
	//if (num_vtime_handlers > 1)
	//  hout << "->" << vtime_handler[current_handler_idx] << "\n";

    }


}

bool associate_virtual_timer_interrupt(vm_t *vm, const L4_ThreadId_t handler_tid)
{
    /*
     * We install a timer thread that ticks with frequency 
     * 10ms / num_vtime_handlers
     */
    
    if (num_handlers == 0)
    {
	vtimer_thread = get_hthread_manager()->create_thread( 
	    hthread_idx_vtimer, PRIO_VTIMER,
	    vtimer);
	
	if( !vtimer_thread )
	{	
	    hout << "Could not install vtimer TID: " 
		 << vtimer_thread->get_global_tid() << '\n';
    
	    return false;
	} 
	L4_KernelInterfacePage_t * kip = (L4_KernelInterfacePage_t *)
	    L4_GetKernelInterface ();
	L4_ThreadId_t s0 = L4_GlobalId (kip->ThreadInfo.X.UserBase, 1);

	if( !L4_Set_Priority(s0, PRIO_ROOTSERVER) )
	{
	    hout << "Error: unable to lower SIGMA0's  priority to " << PRIO_VTIMER-1
		 << ", L4 error code: " << L4_ErrorCode() << '\n';
	    return false;
	}
	if( !L4_Set_Priority(L4_Myself(), PRIO_ROOTSERVER) )
	{
	    hout << "Error: unable to lower SIGMA0's  priority to " << PRIO_VTIMER-1
		 << ", L4 error code: " << L4_ErrorCode() << '\n';
	    return false;
	}
	vtimer_thread->start();
	
    }
    
    /*
     * We install a timer thread that ticks with frequency 
     * 10ms / num_registered_handlers
     */
    for (L4_Word_t h_idx=0; h_idx < num_handlers; h_idx++)
    {
	if (vtime_handler[h_idx].tid == handler_tid)
	{
	    hout << "Vtime handler"
		 << " TID " << handler_tid
		 << " already registered"
		 << "\n";
	    return true;
	}
    }	

    if (num_handlers == MAX_VTIMER_VM)
    {
	hout << "Vtimer reach maximum number of handlers"
	     << " (" << num_handlers << ")"
	     << "\n"; 
	return false;
    }
    
    vtime_handler[num_handlers].vm = vm;
    vtime_handler[num_handlers].state = vm_state_running;
    vtime_handler[num_handlers].tid = handler_tid;
    num_handlers++;
    
    vtimer_period_len = VTIMER_PERIOD_LEN / num_handlers;
    vtimer_period = L4_TimePeriod( vtimer_period_len );
    
    L4_Word_t dummy, errcode;

    errcode = L4_ThreadControl( handler_tid, handler_tid, vtimer_thread->get_global_tid(), 
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

    vm->set_vtimer_tid(L4_ProcessorNo(), vtimer_thread->get_global_tid());

    hout << "Vtimer registered handler " <<  handler_tid
	 << " period " <<  (L4_Word_t) vtimer_period_len
	 << "\n"; 

    return true;
}


bool deassociate_virtual_timer_interrupt(vm_t *vm, const L4_ThreadId_t caller_tid)
{
    hout << "Vtimer unregistering handler unimplemented"
	 << " caller" << caller_tid
	 << "\n"; 
    L4_KDB_Enter("Resourcemon BUG");
    return false;
}

void vtimer_init()
{
    for (L4_Word_t h_idx=0; h_idx < MAX_VTIMER_VM; h_idx++)
	vtime_handler[h_idx].tid = L4_nilthread;
    
    num_handlers = 0;
    vtime_current_handler = 0;
    vtimer_period_len = VTIMER_PERIOD_LEN;
    vtimer_period = L4_TimePeriod( vtimer_period_len );

}

#endif /* defined(cfg_l4ka_vmextensions) */
