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
L4_ThreadId_t s0 = L4_nilthread;
L4_KernelInterfacePage_t * kip = (L4_KernelInterfacePage_t *) 0;

vtime_t vtimers[IResourcemon_max_cpus];


#define MSG_LABEL_PREEMPTION	0xffd0
#define MSG_LABEL_YIELD		0xffd1
#define MSG_LABEL_CONTINUE	0xffd2
#define MSG_LABEL_IRQ		0xfff0

const L4_MsgTag_t hwirqtag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_IRQ } };
const L4_MsgTag_t continuetag = (L4_MsgTag_t) { X: { 0, 0, 0, MSG_LABEL_CONTINUE} }; 
const L4_MsgTag_t acktag = (L4_MsgTag_t) { raw : 0 }; 

static inline void init_root_servers(vtime_t *vtimer)
{
    
    if (!L4_EnablePreemptionMsgs(s0))
    {
	hout << "Error: unable to set SIGMA0's preemption ctrl"
	     << ", L4 error code: " << L4_ErrorCode() << '\n';
	L4_KDB_Enter("VTIMER BUG");
    }
    if (!L4_EnablePreemptionMsgs(roottask))
    {
	hout << "Error: unable to set ROOTTASK's  preemption ctrl"
	     << ", L4 error code: " << L4_ErrorCode() << '\n';
	L4_KDB_Enter("VTIMER BUG");
    }
	 
    if (!L4_ThreadControl (s0, s0, vtimer->myself, s0, (void *) (-1)))
    {
	hout << "Error: unable to set SIGMA0's  scheduler "
	     << " to " << vtimer->thread->get_global_tid()
	     << ", L4 error code: " << L4_ErrorCode() << '\n';    
	L4_KDB_Enter("VTIMER BUG");
    }
    if (!L4_ThreadControl (roottask, roottask, vtimer->myself, s0, (void *) -1))
    {
	hout << "Error: unable to set ROOTTASK's  scheduler "
	     << " to " << vtimer->thread->get_global_tid()
	     << ", L4 error code: " << L4_ErrorCode() << '\n';
	L4_KDB_Enter("VTIMER BUG");
	
    }
}


static void vtimer_thread( 
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
    
    vtime_t *vtimer = &vtimers[L4_ProcessorNo()];
    L4_ThreadId_t to = L4_nilthread;
    L4_Word_t timeouts = L4_Timeouts(L4_ZeroTime, L4_Never);
    L4_ThreadId_t from;
    L4_ThreadId_t ptimer;
    L4_MsgTag_t tag;
    L4_Word_t dummy;

   
    vtimer->mycpu = L4_ProcessorNo();
    vtimer->myself = L4_Myself();
    if (vtimer->mycpu == 0)
	init_root_servers(vtimer);

    ptimer.global.X.thread_no = L4_ThreadIdSystemBase(kip) - L4_NumProcessors(kip) + vtimer->mycpu;
    ptimer.global.X.version = 1;
    
    if (!L4_AssociateInterrupt(ptimer, vtimer->myself))
    {
	hout << "Vtimer error associating timer irq TID: " << ptimer << "\n"; 
	L4_KDB_Enter("VTIMER BUG");
    }
    if (!L4_Schedule(ptimer, ~0UL, ~0UL, PRIO_IRQ, ~0UL, &dummy))
    {
	hout << "Vtimer error setting timer irq's scheduling parameters TID: " << ptimer << "\n"; 
	L4_KDB_Enter("VTIMER BUG");

    }
    
    if (debug_vtimer)
	hout << "Vtimer TID: " << vtimer->myself << "\n"; 
    
    L4_Set_MsgTag(continuetag);
    
    for (;;)
    {
	tag = L4_Ipc( to, L4_anythread, timeouts, &from);
	ASSERT(L4_IpcSucceeded(tag));
	if (L4_IpcFailed(tag))
	{
	    L4_KDB_Enter("Blarb");
	    ASSERT (vtimer->handler[vtimer->current].state == vm_state_idle);
	    to = L4_nilthread;
	    continue;
	}
	
	bool reschedule = false;
	
	
	switch( L4_Label(tag) )
	{
	case MSG_LABEL_IRQ:
	{
	    vtimer->ticks++;
	    ASSERT(from == timer);
	    L4_Set_MsgTag(acktag);
	    tag = L4_Reply(ptimer);
	    ASSERT(!L4_IpcFailed(tag));
	    to = L4_nilthread;
	    if (vtimer->handler[vtimer->current].state == vm_state_idle)
		reschedule = true;

	}
	break;
	case MSG_LABEL_PREEMPTION:
	{
	    /* Got a preemption messsage */
	    if (from == s0 || from == roottask)
	    {
		/* Make root servers high prio */
		to = from;
	    }
	    else 
	    {	
		/* Preemption message from current thread; perform RR scheduling */
		ASSERT (from == vtimer->handler[vtimer->current].tid);
		reschedule = true;
	    }
	}
	break;
	case MSG_LABEL_YIELD:
	{
	    ASSERT(from == vtimer->handler[vtimer->current]);
	    /* 
	     * yield, so fetch dest
	     */
	    L4_ThreadId_t dest;
	    L4_StoreMR(13, &dest.raw);
	    vtimer->handler[vtimer->current].state = vm_state_idle;	
	    to = L4_nilthread;
    
	    if (dest == L4_nilthread || dest == from)
	    {
		for (word_t idx=0; idx < vtimer->num_handlers; idx++)
		    if (vtimer->handler[idx].state != vm_state_idle)
		    {
			vtimer->current = idx;
			to = vtimer->handler[vtimer->current].tid;
		    }
	    }
	    else 
	    {
		/*  verify that it's an IRQ thread  on our own CPU */
		for (word_t idx=0; idx < vtimer->num_handlers; idx++)
		{
		    if (vtimer->handler[idx].tid == dest)
		    {
			vtimer->current = idx;
			to = vtimer->handler[vtimer->current].tid;
			break;
		    }
		}
	    }
	    L4_Set_MsgTag(continuetag);
	}
	break;
	default:
	{
	    hout << "IRQ strange IPC"
		 << " current handler " << vtimer->handler[vtimer->current].tid
		 << " tag " << (void *) tag.raw
		 << "\n"; 
	    L4_KDB_Enter("Vtimer BUG");
	}
	break;
	}


	if (!reschedule)
	    continue;
	
	if (++vtimer->scheduled == vtimer->num_handlers)
	    vtimer->scheduled = 0;
	
	vtimer->current = vtimer->scheduled;
	vtimer->handler[vtimer->current].state = vm_state_running;
	/* Deliver pending vtimer interrupts */
	if (vtimer->ticks - vtimer->handler[vtimer->current].last_tick >= 
	    vtimer->handler[vtimer->current].period_len)
	{
	    vtimer->handler[vtimer->current].last_tick = vtimer->ticks;
	    vtimer->handler[vtimer->current].vm->
		set_vtimer_irq_pending(vtimer->mycpu, 
				       vtimer->handler[vtimer->current].idx);
	}
	to = vtimer->handler[vtimer->current].tid;
	L4_Set_MsgTag(continuetag);


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
	if (cpu == 0)
	{
	    if (!L4_Set_Priority(s0, PRIO_ROOTSERVER))
	    {
		hout << "Error: unable to set SIGMA0's "
		     << " prio to " << PRIO_ROOTSERVER
		     << ", L4 error code: " << L4_ErrorCode() << '\n';
		L4_KDB_Enter("VTIMER BUG");
	    }
	    if (!L4_Set_Priority(roottask, PRIO_ROOTSERVER))
	    {
		hout << "Error: unable to set ROOTTASK's"
		     << " prio to" << PRIO_ROOTSERVER
		     << ", L4 error code: " << L4_ErrorCode() << '\n';
		L4_KDB_Enter("VTIMER BUG");
	    }
	 
	}
	vtimer->thread = get_hthread_manager()->create_thread( 
	    (hthread_idx_e) (hthread_idx_vtimer + cpu), PRIO_VTIMER,
	    vtimer_thread);
	
	if( !vtimer->thread )
	{	
	    hout << "Could not install vtimer TID: " 
		 << vtimer->thread->get_global_tid() << '\n';
    
	    return false;
	} 

	if (!L4_Set_ProcessorNo(vtimer->thread->get_global_tid(), cpu))
	{
	    hout << "Error: unabl1e to set vtimer thread's cpu to " << cpu
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
    kip = (L4_KernelInterfacePage_t *) L4_GetKernelInterface ();
    roottask = L4_Myself();
    s0 = L4_GlobalId (kip->ThreadInfo.X.UserBase, 1);
	    
    for (L4_Word_t cpu=0; cpu < IResourcemon_max_cpus; cpu++)
    {
	for (L4_Word_t h_idx=0; h_idx < MAX_VTIMER_VM; h_idx++)
	{
	    vtimers[cpu].handler[h_idx].tid = L4_nilthread;
	    vtimers[cpu].handler[h_idx].idx = 0;
	}
	vtimers[cpu].ticks = 0;
	vtimers[cpu].num_handlers = 0;
	vtimers[cpu].current = 0;
	vtimers[cpu].scheduled = 0;
		
    }

}

#endif /* defined(cfg_l4ka_vmextensions) */
