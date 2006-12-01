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

L4_ThreadId_t vtime_handler[MAX_VTIMER_VM];
L4_Word_t     num_vtime_handlers;
L4_Word_t     current_handler_idx;
L4_Word64_t   vtimer_period_len;
L4_Time_t vtimer_period;
hthread_t *vtimer_thread;

#define MSG_LABEL_PREEMPTION	0xffd0
#define MSG_LABEL_YIELD		0xffd1
#define MSG_LABEL_CONTINUE	0xffd2
#define MSG_LABEL_IRQ		0xfff0

static void vtimer( 
	void *param ATTR_UNUSED_PARAM,
	hthread_t *htread ATTR_UNUSED_PARAM)
{
    hout << "Vtimer TID: " << L4_Myself() << "\n"; 

    L4_Sleep( vtimer_period );
    L4_ThreadId_t myself = L4_Myself();
    L4_ThreadId_t current_handler = vtime_handler[current_handler_idx];
    L4_ThreadId_t to = current_handler, from = L4_nilthread;
    L4_Word_t timeouts = L4_Timeouts(L4_ZeroTime, vtimer_period);
    L4_MsgTag_t tag;
    const L4_MsgTag_t hwirqtag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_IRQ } };
    const L4_MsgTag_t continuetag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_CONTINUE} }; 
    
    L4_Set_MsgTag(hwirqtag);
    
    for (;;)
    {
	L4_Set_TimesliceReceiver(current_handler);
	tag = L4_Ipc( to, current_handler, timeouts, &from );
	
	if (!L4_IpcFailed(tag))
	{
	    /* Got a message, so irq thread preempted before timeout*/
	    if (L4_Label(tag) == MSG_LABEL_YIELD)
	    {
		/* yield, so fetch dest
		 * actually one should verify that it's an IRQ thread
		 */
		L4_ThreadId_t dest;
		L4_StoreMR(13, &dest.raw);
		//hout << "IRQ Yield " << from << " -> " << dest << "\n";
		L4_Set_MsgTag(continuetag);
		
		if (dest != L4_nilthread)
		{
		    to = current_handler = dest;
		    continue;
		}
	    }
	    else
	    {
		hout << "IRQ strange IPC"
		     << " current handler " << current_handler
		     << " label " << (void *) L4_Label(tag) << "\n"; 
		L4_KDB_Enter("Vtimer BUG");
	    }
	}
	else if( (L4_ErrorCode() & 0xf) == 2 ) 
	{
	    /* send timeout, i.e., the handler preempted.
	     * receive preemption message (if any) and restart
	     */
	    tag = L4_Receive(current_handler, L4_ZeroTime);

	    if (L4_IpcFailed(tag))
	    {
		//hout << "IRQ thread running, sleep\n";
		to = L4_nilthread;
		continue;
	    }
	    else if (L4_Label(tag) == MSG_LABEL_PREEMPTION)
	    {
		//hout << "IRQ preempted, reply again\n";
		L4_Set_MsgTag(hwirqtag);
		continue;
	    }
	    else
	    {
		hout << "IRQ send timeout and unexpected IPC"
		     << " from " << from
		     << " with label " << (void *) L4_Label(tag) << "\n";
		L4_KDB_Enter("VTimer BUG");
	    }
    
	}
	/* receive timeout, i.e., we woke up; schedule the next irq thread
	 * 
	 */
	
	if (++current_handler_idx == num_vtime_handlers)
	    current_handler_idx = 0;
	
	current_handler = vtime_handler[current_handler_idx];
	to = current_handler;
	L4_Set_MsgTag(hwirqtag);

	//if (num_vtime_handlers > 1)
	//  hout << "next " << vtime_handler[current_handler] << "\n";

    }


}

bool associate_virtual_timer_interrupt(vm_t *vm, const L4_ThreadId_t handler_tid)
{
    /*
     * We install a timer thread that ticks with frequency 
     * 10ms / num_vtime_handlers
     */
    
    if (num_vtime_handlers == 0)
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
    for (L4_Word_t h_idx=0; h_idx < num_vtime_handlers; h_idx++)
    {
	if (vtime_handler[h_idx] == handler_tid)
	{
	    hout << "Vtime handler"
		 << " TID " << handler_tid
		 << " already registered"
		 << "\n";
	    return true;
	}
    }	

    if (num_vtime_handlers == MAX_VTIMER_VM)
    {
	hout << "Vtimer reach maximum number of handlers"
	     << " (" << num_vtime_handlers << ")"
	     << "\n"; 
	return false;
    }
    
    vtime_handler[num_vtime_handlers++] = handler_tid;
    vtimer_period_len = VTIMER_PERIOD_LEN / num_vtime_handlers;
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
	vtime_handler[h_idx] = L4_nilthread;
    
    num_vtime_handlers = 0;
    current_handler_idx = 0;
    vtimer_period_len = VTIMER_PERIOD_LEN;
    vtimer_period = L4_TimePeriod( vtimer_period_len );

}

#endif /* defined(cfg_l4ka_vmextensions) */
