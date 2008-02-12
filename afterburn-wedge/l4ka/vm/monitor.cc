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
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(monitor.h)
#include INC_WEDGE(hthread.h)

void monitor_loop( vcpu_t & vcpu, vcpu_t &activator )
{
    printf( "Entering monitor loop, TID %t\n", L4_Myself());
    L4_ThreadId_t tid = vcpu.irq_gtid;

    vcpu.irq_info.mr_save.set_propagated_reply(L4_Pager()); 	
    vcpu.irq_info.mr_save.load();
    L4_Reply(vcpu.irq_gtid);
    
    L4_Word_t timeouts = default_timeouts;

    
    vcpu.main_info.mr_save.load();
    tid = vcpu.main_gtid;
    for (;;) 
    {
	L4_MsgTag_t tag = L4_Ipc( tid, L4_anythread, timeouts, &tid );

	if( L4_IpcFailed(tag) ) {
	    if (tid != L4_nilthread)
	    {
		printf( "Failed sending message %x to TID %t\n", tag.raw, tid);
		DEBUGGER_ENTER("VMEXT BUG");
	    }
	    tid = L4_nilthread;
	    continue;
	}

	switch( L4_Label(tag) )
	{
	case msg_label_pfault_start ... msg_label_pfault_end:
	{
	    thread_info_t *vcpu_info = backend_handle_pagefault(tag, tid);
	    if( !vcpu_info )
	    {
		L4_Word_t ip;
		L4_StoreMR( OFS_MR_SAVE_EIP, &ip );
		printf( "Unhandled monitor pagefault TID %t ip %x\n", tid, ip);
		panic();
		tid = L4_nilthread;
	    }
	    else
		vcpu_info->mr_save.load();
	}
	break;
	case msg_label_exception:
	{
	    L4_Word_t ip;
	    L4_StoreMR( OFS_MR_SAVE_EIP, &ip );
	    printf( "Unhandled monitor exception TID %t ip %x\n", tid, ip);
	    panic();
	}
	break;
	case msg_label_thread_create:
	{
	    vcpu_t *tvcpu;
	    L4_Word_t stack_bottom;
	    L4_Word_t stack_size;
	    L4_Word_t prio;
	    hthread_func_t start_func;
	    L4_ThreadId_t pager_tid;
	    void *start_param;
	    void *tlocal_data;
	    L4_Word_t tlocal_size;

	    msg_thread_create_extract((void**) &tvcpu, &stack_bottom, &stack_size, &prio, 
				      (void *) &start_func, &pager_tid, &start_param, &tlocal_data, &tlocal_size);		       

	    hthread_t *hthread = get_hthread_manager()->create_thread(tvcpu, stack_bottom, stack_size, prio, 
								      start_func, pager_tid, start_param, 
								      tlocal_data, tlocal_size);
	    
	    msg_thread_create_done_build(hthread);
	}
	break;
	default:
	    printf( "Unhandled message %x from TID %t\n", tag.raw, tid);
	    DEBUGGER_ENTER("monitor: unhandled message");


	}
    }
}

