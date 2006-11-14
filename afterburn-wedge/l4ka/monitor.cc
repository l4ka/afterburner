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
#include <l4/ipc.h>

#include INC_ARCH(page.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(vcpulocal.h)
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
	con << "Invalid page fault message from TID " << tid << '\n';
	return NULL;
    }
    
    
    if (tid == vcpu.main_gtid)
	ti = &vcpu.main_info;
    else if (tid == vcpu.irq_gtid)
	ti = &vcpu.irq_info;
#if defined(CONFIG_VSMP)
    else if (vcpu.is_bootstrapping_other_vcpu() &&
	     tid == get_vcpu(vcpu.get_bootstrapped_cpu_id()).monitor_gtid)
	ti = &get_vcpu(vcpu.get_bootstrapped_cpu_id()).main_info;
#endif
    else 
    {
	con << "Invalid page fault message from TID " << tid << '\n';
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
    con << "Entering monitor loop, TID " << L4_Myself() << '\n';

    L4_ThreadId_t tid = L4_nilthread;
    
    for (;;) {
	L4_MsgTag_t tag = L4_ReplyWait( tid, &tid );

	if( L4_IpcFailed(tag) ) {
	    if (tid != L4_nilthread)
	    {
		con << "Failed sending message " << (void *)tag.raw
		    << " to TID " << tid << '\n';
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
		    tid = L4_nilthread;
		else
		    vcpu_info->mr_save.load_mrs();
		break;

	    case msg_label_exception:
		L4_Word_t ip;
		L4_StoreMR( 1, &ip );
		con << "Unhandled kernel exception, ip " << (void *)ip << '\n';
		panic();
		
#if defined(CONFIG_L4KA_VMEXTENSIONS)
	    case msg_label_preemption:
	    {
		if (tid == vcpu.main_gtid)
		{	
		    irq_lock.lock();
		    vcpu.main_info.mr_save.store_mrs(tag);
		    if (debug_preemption)
			con << "kernel thread sent preemption IPC"
			    << " tid " << tid << "\n";
		    
		    if (backend_async_irq_deliver( get_intlogic() ))
		    {
			tid = vcpu.main_gtid;
			if (debug_preemption)
			    con << "send preemption reply to kernel"
				<< " tid " << tid << "\n";
			vcpu.main_info.mr_save.load_preemption_reply();
			vcpu.main_info.mr_save.load_mrs();
		    }
		    else
			tid = L4_nilthread;
		    irq_lock.unlock();
			
		    break;
		}
		con << "Unhandled preemption message " << (void *)tag.raw
		    << " from TID " << tid << '\n';
		L4_KDB_Enter("monitor: unhandled preemption  message");
		
	    }
#endif
#if defined(CONFIG_VSMP)
	    case msg_label_startup_monitor:
	    {
		L4_Word_t vcpu_id, monitor_ip, monitor_sp;

		msg_startup_monitor_extract( &vcpu_id, &monitor_ip, &monitor_sp);
		// Begin startup, monitor will end it as soon as it has finished	    
		vcpu.bootstrap_other_vcpu(vcpu_id);	    
		
		ASSERT(vcpu_id < CONFIG_NR_VCPUS);
		L4_ThreadId_t monitor = get_vcpu(vcpu_id).monitor_gtid;
	    
		con << "starting up monitor " << monitor << " VCPU " << vcpu_id
		    << " (ip " << (void *) monitor_ip
		    << ", sp " << (void *) monitor_sp
		    << ")\n";
		msg_startup_build(monitor_ip, monitor_sp);
		tag = L4_Send( monitor );
	    
		L4_Msg_t msg;	
		L4_Clear( &msg );
		L4_Append( &msg, (L4_Word_t) tag.raw );    
		L4_Load( &msg );
		break;
	    }
	    case msg_label_startup_monitor_done:
	    {
		ASSERT(tid == vcpu.irq_gtid);
		
		con << "finished starting up monitor " << L4_Myself() 
		    << " VCPU " << get_vcpu().cpu_id
		    << "\n";
	    
		if (vcpu.cpu_id != activator.cpu_id)
		{
		    msg_startup_monitor_done_build();
		    tid = activator.main_gtid;
		}
		else
		    tid = L4_nilthread;

		break;
	    }

#endif  /* defined(CONFIG_VSMP) */ 
	    default:
		con << "Unhandled message " << (void *)tag.raw
		    << " from TID " << tid << '\n';
		L4_KDB_Enter("monitor: unhandled message");


	}
    }
}

