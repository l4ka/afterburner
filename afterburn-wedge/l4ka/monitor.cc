/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4-common/monitor.cc
 * Description:   The monitor thread, which handles wedge faults
 *                (primarily page faults).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ********************************************************************/

#include <l4/thread.h>
#include <l4/schedule.h>
#include <l4/ipc.h>

#include INC_ARCH(page.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)

static const bool debug_pfault=0;
static const bool debug_preemption=0;


static thread_info_t *handle_pagefault( L4_MsgTag_t tag, L4_ThreadId_t tid )
{
    word_t map_addr;
    L4_MapItem_t map_item;
    word_t map_rwx, map_page_bits;
    vcpu_t &vcpu = get_vcpu();
    thread_info_t *ti = NULL;
    
    if( L4_UntypedWords(tag) != 2 ) {
	con << "Bogus page fault message from TID " << tid << '\n';
	return NULL;
    }
    
    if (tid == vcpu.main_gtid)
	ti = &vcpu.main_info;
    else if (tid == vcpu.irq_gtid)
	ti = &vcpu.irq_info;
#if defined(CONFIG_VSMP)
    else if (vcpu.is_bootstrapping_other_vcpu()
	    && tid == get_vcpu(vcpu.get_bootstrapped_cpu_id()).monitor_gtid)
	    ti = &get_vcpu(vcpu.get_bootstrapped_cpu_id()).monitor_info;
#endif
    else 
    {
	con << "Invalid page fault message from bogus TID " << tid << '\n';
	return NULL;
    }
    ti->mr_save.store_mrs(tag);
    
    if (debug_pfault)
    { 
	con << "pfault, VCPU " << vcpu.cpu_id  
	    << " addr: " << (void *) ti->mr_save.get_pfault_addr()
	    << ", ip: " << (void *) ti->mr_save.get_pfault_ip()
	    << ", rwx: " << (void *)  ti->mr_save.get_pfault_rwx()
	    << ", TID: " << tid << '\n'; 
    }  
    
    bool complete = 
	backend_handle_pagefault(tid, map_addr, map_page_bits, map_rwx, ti);
		
    if (complete)
	map_item = L4_MapItem( L4_Nilpage, 0 );
    else
	map_item = L4_MapItem( 
	    L4_FpageAddRights(L4_FpageLog2(map_addr, map_page_bits), map_rwx),
	    ti->mr_save.get_pfault_addr());

    ti->mr_save.load_pfault_reply(map_item);
    return ti;
}

void monitor_loop( vcpu_t & vcpu, vcpu_t &activator )
{
    con << "Entering monitor loop, TID " << L4_Myself() << "\n";
    L4_ThreadId_t tid = vcpu.irq_gtid;
    vcpu.irq_info.mr_save.set_propagated_reply(L4_Pager()); 	
    vcpu.irq_info.mr_save.load_mrs();
    L4_Word_t timeouts = default_timeouts;
    
    for (;;) 
    {
	L4_MsgTag_t tag = L4_Ipc( tid, L4_anythread, timeouts, &tid );

	if( L4_IpcFailed(tag) ) {
	    if (tid != L4_nilthread)
	    {
		con << "Failed sending message " << (void *)tag.raw
		    << " to TID " << tid << "\n";
		DEBUGGER_ENTER();
	    }
	    tid = L4_nilthread;
	    continue;
	}

	switch( L4_Label(tag) )
	{
	    case msg_label_pfault_start ... msg_label_pfault_end:
		thread_info_t *vcpu_info = handle_pagefault(tag, tid);
		if( !vcpu_info )
		{
		    L4_Word_t ip;
		    L4_StoreMR( OFS_MR_SAVE_EIP, &ip );
		    con << "Unhandled monitor pagefault, ip " << (void *)ip << "\n";
		    panic();
		    tid = L4_nilthread;
		}
		else
		    vcpu_info->mr_save.load_mrs();
		break;

	    case msg_label_exception:
		L4_Word_t ip;
		L4_StoreMR( OFS_MR_SAVE_EIP, &ip );
		con << "Unhandled monitor exception, ip " << (void *)ip << "\n";
		panic();
		
#if defined(CONFIG_L4KA_VMEXTENSIONS)
	    case msg_label_preemption:
	    {
 	        ASSERT(tid == vcpu.main_gtid);
		if (debug_preemption)
		    con << "main thread sent preemption IPC"
			<< " ip " << (void *) vcpu.main_info.mr_save.get_preempt_ip()
			<< "\n";
		
		irq_lock.lock();
		vcpu.main_info.mr_save.store_mrs(tag);
		backend_async_irq_deliver(get_intlogic());
		vcpu.main_info.mr_save.load_preemption_reply();
		vcpu.main_info.mr_save.load_mrs();
		tid = vcpu.main_gtid;
		irq_lock.unlock();
		break;
	    }
	    case msg_label_preemption_yield:
	    {
		ASSERT(tid == vcpu.main_gtid);	
		irq_lock.lock();
		vcpu.main_info.mr_save.store_mrs(tag);
		L4_ThreadId_t dest = vcpu.main_info.mr_save.get_preempt_target();
		    
		if (debug_preemption)
		{
		    con << "main thread sent yield IPC"
			<< " dest " << dest
			<< "\n";
		}

		if (dest == vcpu.monitor_gtid
			&& backend_async_irq_deliver(get_intlogic()))
		{
		    vcpu.main_info.mr_save.load_preemption_reply();
		    vcpu.main_info.mr_save.load_mrs();
		    tid = vcpu.main_gtid;
		}
		else 
		{
		    vcpu.monitor_info.mr_save.load_yield_msg(dest);
		    vcpu.monitor_info.mr_save.load_mrs();
		    tid = vcpu.irq_gtid;
		}
		irq_lock.unlock();
		break;
		    
	    }
#endif
	    case msg_label_startup_monitor:
	    {
		ASSERT (tid == vcpu.irq_gtid);
		    
#if defined(CONFIG_L4KA_VMEXTENSIONS)
		L4_Word_t preemption_control = L4_PREEMPTION_CONTROL_MSG;
		L4_Word_t time_control = (L4_Never.raw << 16) | L4_Never.raw;
		L4_ThreadId_t scheduler = vcpu.irq_gtid;
#else
		L4_Word_t preemption_control = ~0UL;
		L4_Word_t time_control = ~0UL;
		L4_ThreadId_t scheduler = vcpu.monitor_gtid;
#endif
		L4_Word_t monitor_prio = vcpu.get_vcpu_max_prio() + CONFIG_PRIO_DELTA_MONITOR;
		L4_Word_t dummy;
		L4_Error_t errcode;
		    
		if (!L4_Schedule(L4_Myself(), time_control, ~0UL, monitor_prio, 
				preemption_control, &dummy))
		    PANIC( "Failed to set scheduling parameters for monitor thread");
		
		errcode = ThreadControl( vcpu.monitor_gtid, vcpu.monitor_gtid, 
			scheduler, L4_nilthread, (word_t) -1 );
		if (errcode != L4_ErrOk)
		    PANIC("Error: unable to set monitor thread's scheduler "
			    << "L4 error: " << L4_ErrString(errcode) 
			    << "\n");
		
		tid = vcpu.main_gtid;
		vcpu.main_info.mr_save.load_mrs();
		break;

	    }
	    default:
		con << "Unhandled message " << (void *)tag.raw
		    << " from TID " << tid << '\n';
		L4_KDB_Enter("monitor: unhandled message");


	}
    }
}

