/*********************************************************************
 *                
 * Copyright (C) 2006-2007,  Karlsruhe University
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
#include <resourcemon/vm.h>
#include <resourcemon/vtime.h>
#include <resourcemon/resourcemon.h>

#if defined(cfg_l4ka_vmextensions)


L4_ThreadId_t roottask = L4_nilthread;

vtime_t vtimers[IResourcemon_max_cpus];


#define MSG_LABEL_PREEMPTION	0xffd0
#define MSG_LABEL_YIELD		0xffd1
#define MSG_LABEL_CONTINUE	0xffd2
#define MSG_LABEL_IRQ		0xfff0

const L4_MsgTag_t hwirqtag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_IRQ } };
const L4_MsgTag_t continuetag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_CONTINUE} }; 
const L4_MsgTag_t acktag = (L4_MsgTag_t) { raw : 0 }; 

static void vtimer_thread( 
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
    if (debug_vtimer)
	hout << "Vtimer TID: " << L4_Myself() << "\n"; 

    
    L4_Word_t cpu = L4_ProcessorNo();
    vtime_t *vtimer = &vtimers[cpu];
    L4_ThreadId_t myself = L4_Myself();
    L4_Word_t     current = vtimer->current_handler;
    L4_ThreadId_t to = vtimer->handler[current].tid;
    L4_ThreadId_t from;
    L4_ThreadId_t timer;
    L4_MsgTag_t tag;
    L4_Word_t timeouts = L4_Timeouts(L4_ZeroTime, L4_Never);
    L4_Word_t dummy;

    L4_Word_t tid_system_base = L4_ThreadIdSystemBase(L4_GetKernelInterface()); 
    timer.global.X.thread_no = tid_system_base - L4_NumProcessors(L4_GetKernelInterface()) + cpu;
    timer.global.X.version = 1;
    
    if (!L4_AssociateInterrupt(timer, myself))
    {
	hout << "Vtimer error associating timer irq TID: " << L4_Myself() << "\n"; 
	L4_KDB_Enter("VTIMER BUG");

    }
    if (!L4_Schedule(timer, ~0UL, cpu, PRIO_IRQ, ~0UL, &dummy))
    {
	hout << "Vtimer error setting timer irq's scheduling parameters TID: " << L4_Myself() << "\n"; 
	L4_KDB_Enter("VTIMER BUG");

    }
	
    L4_Set_MsgTag(hwirqtag);
   
    for (;;)
    {
	L4_Set_TimesliceReceiver(vtimer->handler[current].tid);
	tag = L4_Ipc( to, L4_anythread, timeouts, &from);
	bool reschedule = true;
	
	if ( L4_IpcFailed(tag) )
	{
	    ASSERT( (L4_ErrorCode() & 0xf) == 2 );

	    /* to not waiting for us, i.e. 
	     * - running with a message from main thread
	     * - serviced by roottask/other vm
	     * - polling for us (preemption msg)
	     */
	    to = L4_nilthread;
	    continue;
	}
		
	switch( L4_Label(tag) )
	{
	case MSG_LABEL_IRQ:
	{
	    vtimer->ticks++;
	    ASSERT(from == timer);
	    L4_Set_MsgTag(acktag);
	    tag = L4_Reply(timer);
	    ASSERT(!L4_IpcFailed(tag));
	}
	break;
	case MSG_LABEL_PREEMPTION:
	{
	    /* Got a preemption messsage */
	    if (vtimer->num_handlers > 1)
	    {
		ASSERT (from != vtimer->handler[current].tid);
		to = L4_nilthread;
	    }
	    else 
	    {	
		ASSERT (from == vtimer->handler[current].tid);
		to = from;
		L4_Set_MsgTag(continuetag);
	    }
	    reschedule = false;
	}
	break;
	case MSG_LABEL_YIELD:
	{
	    ASSERT(from == vtimer->handler[current]);
	    /* 
	     * yield, so fetch dest
	     * actually we should verify that it's an IRQ thread
	     */
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
			to =  vtimer->handler[current].tid;
		    }
	    }
	    else 
		to = dest;
	    
	    L4_Set_MsgTag(continuetag);
	    reschedule = false;
	}
	break;
	default:
	{
	    hout << "IRQ strange IPC"
		 << " current handler " << vtimer->handler[current].tid
		 << " label " << (void *) L4_Label(tag) << "\n"; 
	    L4_KDB_Enter("Vtimer BUG");
	}
	break;
	}
	
	if (!reschedule)
	    continue;
	
	if (++vtimer->current_handler == vtimer->num_handlers)
	    vtimer->current_handler = 0;
	
	current = vtimer->current_handler;
	vtimer->handler[current].state = vm_state_running;
	if (vtimer->ticks - vtimer->handler[current].last_tick >= vtimer->handler[current].period_len)
	{
	    vtimer->handler[current].last_tick = vtimer->ticks;
	    vtimer->handler[current].vm->set_vtimer_irq_pending(cpu, vtimer->handler[current].idx);
	}
	to = vtimer->handler[current].tid;
	L4_Set_MsgTag(hwirqtag);

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
	    (hthread_idx_e) (hthread_idx_vtimer + cpu), PRIO_VTIMER,
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
	
	roottask = L4_Myself();
	if( !L4_Set_Priority(roottask, PRIO_ROOTSERVER) )
	{
	    hout << "Error: unable to set SIGMA0's  priority to " << PRIO_ROOTSERVER
		 << ", L4 error code: " << L4_ErrorCode() << '\n';
	    return false;
	}
	
	if (!L4_Set_ProcessorNo(vtimer->thread->get_global_tid(), cpu))
	{
	    hout << "Error: unable to set vtimer thread's cpu to " << cpu
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
	hout << "Vtimer reached maximum number of handlers"
	     << " (" << vtimer->num_handlers << ")"
	     << "\n"; 
	return false;
    }
    
    vtimer->handler[vtimer->num_handlers].vm = vm;
    vtimer->handler[vtimer->num_handlers].state = vm_state_running;
    vtimer->handler[vtimer->num_handlers].tid = handler_tid;
    vtimer->handler[vtimer->num_handlers].idx = handler_idx;
    /* jsXXX: make configurable */
    vtimer->handler[vtimer->num_handlers].period_len = 10;
    vtimer->handler[vtimer->num_handlers].last_tick = 0;
    vtimer->num_handlers++;
    

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
		     cpu, ~0UL, L4_PREEMPTION_CONTROL_MSG, &dummy))
    {
	hout << "Vtimer error: failed to set scheduling parameters for irq thread\n";
	L4_KDB_Enter("VTimer BUG");
    }

    vm->set_vtimer_tid(cpu, vtimer->thread->get_global_tid());

    if (debug_vtimer)
	hout << "Vtimer registered handler " <<  handler_tid
	     << " vtimer_tid " <<  vtimer->thread->get_global_tid()
	     << " cpu " <<  (L4_Word_t) cpu
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
	vtimers[cpu].ticks = 0;
	vtimers[cpu].num_handlers = 0;
	vtimers[cpu].current_handler = 0;
    }

}

#endif /* defined(cfg_l4ka_vmextensions) */
