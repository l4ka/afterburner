/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4ka/monitor.cc
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
#include <l4/message.h>
#include <l4/ipc.h>

#include INC_WEDGE(vm.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(debug.h)
#include INC_ARCH(page.h)
#include INC_ARCH(intlogic.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(vt/message.h)

#include <ia32/page.h>


void monitor_loop( vcpu_t &unused1, vcpu_t &unused2 )
{
    vcpu_t &vcpu = get_vcpu();
    con << "Entering monitor loop, TID " << L4_Myself() << '\n';

    L4_ThreadId_t tid = L4_nilthread;
    thread_info_t *ti = NULL;    
    for (;;) {
	L4_MsgTag_t tag = L4_ReplyWait( tid, &tid );
	
	// is it an interrupt delivery?
	if( msg_is_vector( tag ) ) {
	    L4_Word_t vector;
	    msg_vector_extract( &vector );
	    // con << "Received int " << vector << "\n";
	  	    
	    if( !vcpu.main_info.deliver_interrupt() ) {
		// not handled immediately
		tid = L4_nilthread;
	    }
	    else {
		tid = vcpu.main_gtid;
	    }
	    
	    continue;
	}	
	
	switch( L4_Label(tag) >> 4 )
	{
	    case L4_LABEL_PFAULT:
		// handle page fault
		// assume that if returns true, then MRs contain the mapping
		// message
		ti = backend_handle_pagefault(tag, tid);
		ASSERT(ti);
		ti->mr_save.load();
		break;

	    case L4_LABEL_EXCEPT:
		L4_Word_t ip;
		L4_StoreMR( 1, &ip );
		con << "Unhandled kernel exception, ip " << (void *)ip << '\n';
		panic();
		tid = L4_nilthread;
		break;

	    case L4_LABEL_REGISTER_FAULT:
	    case L4_LABEL_INSTRUCTION_FAULT:
	    case L4_LABEL_EXCEPTION_FAULT:
	    case L4_LABEL_IO_FAULT:
	    case L4_LABEL_MSR_FAULT:
	    case L4_LABEL_DELAYED_FAULT:
	    case L4_LABEL_IMMEDIATE_FAULT:
		// check if vcpu valid and request comes from the right thread
		ASSERT(tid == vcpu.main_gtid);
		
		// process message
		if( !vcpu.main_info.process_vfault_message() ) 
		{
		    tid = L4_nilthread;
		    break;
		}
		
		// do not reply when waiting for an interrupt
		if( vcpu.main_info.state != thread_state_running ) {
		    tid = L4_nilthread;
		}
		    
		break;
		
	    case L4_LABEL_INTR:
		con << "Unhandled interrupt " << (void *)tag.raw
		    << " from TID " << tid << '\n';
		L4_KDB_Enter("monitor: unhandled interrupt");
		tid = L4_nilthread;
		break;
		
	    default:

		con << "Unhandled message " << (void *)tag.raw
		    << " from TID " << tid << '\n';
		L4_KDB_Enter("monitor: unhandled message");
		tid = L4_nilthread;
	}
    }
}

