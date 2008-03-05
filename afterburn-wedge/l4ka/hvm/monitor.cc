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
#include <debug.h>
#include INC_ARCH(page.h)
#include INC_ARCH(intlogic.h)
#include <console.h>
#include INC_WEDGE(hvm/message.h)

#include <ia32/page.h>


void monitor_loop( vcpu_t &unused1, vcpu_t &unused2 )
{
    vcpu_t &vcpu = get_vcpu();
    printf( "Entering monitor loop, TID %t\n", L4_Myself());

    // drop monitor below irq handler priority
    printf( "Decreasing monitor priority\n");
    L4_Set_Priority( L4_Myself(), resourcemon_shared.prio + CONFIG_PRIO_DELTA_IRQ_HANDLER -1);

    L4_ThreadId_t tid = L4_nilthread;
    thread_info_t *ti = NULL;    
    intlogic_t &intlogic = get_intlogic();

    //dbg_irq(1);
    //intlogic.set_irq_trace(14);
    //intlogic.set_irq_trace(15);

    for (;;) {
	
	backend_async_irq_deliver( intlogic );
	L4_MsgTag_t tag = L4_ReplyWait( tid, &tid );
	
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
		printf( "Unhandled kernel exception, ip %x\n", ip);
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

		vcpu.main_info.mr_save.store_mrs(tag);
		
		// process message
		if( !backend_handle_vfault_message() ) 
		    tid = L4_nilthread;
		
		break;
		
	    case L4_LABEL_INTR:
		printf( "Unhandled interrupt tag %x from %t\n", tag.raw, tid);
		DEBUGGER_ENTER("monitor: unhandled interrupt");
		tid = L4_nilthread;
		break;
		
	    case L4_LABEL_VIRT_ERROR:
		L4_Word_t basic_exit_reason;
		L4_StoreMR(1, &basic_exit_reason);
		
		printf( "Virtualization error: from %t tag %x basic exit reason %d\n",
			tid, tag.raw, basic_exit_reason);
		
		break;
	
	    default:
		printf( "Unhandled message tag %x from %t\n", tag.raw, tid);
		DEBUGGER_ENTER("monitor: unhandled message");
		tid = L4_nilthread;
	}
    }
}

