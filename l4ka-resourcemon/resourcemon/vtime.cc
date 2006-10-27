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
L4_Word_t     current_handler;
L4_Word64_t   vtimer_period_len;
L4_Time_t     vtimer_period;


static void vtimer( 
	void *param ATTR_UNUSED_PARAM,
	hthread_t *htread ATTR_UNUSED_PARAM)
{
    hout << "Vtimer TID: " << L4_Myself() << "\n"; 
    L4_Sleep( vtimer_period );
    L4_ThreadId_t myself = L4_Myself();
    L4_ThreadId_t dummy;
    
    for (;;)
    {
	L4_Set_MsgTag((L4_MsgTag_t) { X: { 0, 0, 0, 0xfff0 } });
	if (!L4_IpcFailed(L4_Ipc (vtime_handler[current_handler],
				  myself,
				  L4_Timeouts (L4_ZeroTime, vtimer_period),
				  &dummy)))
	    L4_KDB_Enter("VTimer Bug");
	    
	// If send timeout, sleep again, donate timeslice to handler
	if( (L4_ErrorCode() & 0xf) == 2 )
	{
	    //hout << "*";
	    L4_KDB_Enter("VTimer");
	    L4_Set_TimesliceReceiver(vtime_handler[current_handler]);
	    L4_Sleep(vtimer_period);
	}
	
	if (++current_handler == num_vtime_handlers)
	    current_handler = 0;
    }

	

}

bool associate_virtual_timer_interrupt(const L4_ThreadId_t handler_tid)
{
    /*
     * We install a timer thread that ticks with frequency 
     * 10ms / num_vtime_handlers
     */
    
    if (num_vtime_handlers == 0)
    {
	hthread_t *vtimer_thread = get_hthread_manager()->create_thread( 
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

#if 1
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
#endif
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
    
    hout << "Vtimer registered handler " <<  handler_tid
	 << " period " <<  (L4_Word_t) vtimer_period_len
	 << "\n"; 

    return true;
}


bool deassociate_virtual_timer_interrupt(const L4_ThreadId_t caller_tid)
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
    current_handler = 0;
    vtimer_period_len = VTIMER_PERIOD_LEN;
    vtimer_period = L4_TimePeriod( vtimer_period_len );

}

#endif /* defined(cfg_l4ka_vmextensions) */
