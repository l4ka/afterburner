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

#include <debug.h>
#include <console.h>
#include <l4/thread.h>
#include <l4/message.h>
#include <l4/ipc.h>

#include INC_WEDGE(vm.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(backend.h)
#include INC_ARCH(page.h)
#include INC_ARCH(intlogic.h)



void monitor_loop( vcpu_t &unused1, vcpu_t &unused2 )
{
    vcpu_t &vcpu = get_vcpu();
    printf( "Entering monitor loop, TID %t\n", L4_Myself());

    // drop monitor below irq handler priority
    printf( "Decreasing monitor priority\n");
    L4_Set_Priority( L4_Myself(), resourcemon_shared.prio + CONFIG_PRIO_DELTA_IRQ_HANDLER -1);

    L4_ThreadId_t tid = L4_nilthread;
    intlogic_t &intlogic = get_intlogic();

    //dbg_irq(1);
    //intlogic.set_irq_trace(14);
    //intlogic.set_irq_trace(15);

    for (;;) {
	
	backend_async_irq_deliver( intlogic );
	L4_MsgTag_t tag = L4_ReplyWait( tid, &tid );
	
	switch( L4_Label(tag) )
	{
	case msg_label_pfault_start ... msg_label_pfault_end:
	    thread_info_t *vcpu_info = backend_handle_pagefault(tag, tid);
	    ASSERT(vcpu_info);
	    vcpu_info->mr_save.load();
	    break;
	    
	case msg_label_exception:
	    ASSERT (tid == vcpu.main_ltid || tid == vcpu.main_gtid);
	    vcpu.main_info.mr_save.store(tag);
	    printf( "unhandled main exception %d, ip %x\n", 
		    vcpu.main_info.mr_save.get_exc_number(),
		    vcpu.main_info.mr_save.get_exc_ip());
	    vcpu.main_info.mr_save.dump(debug_id_t(0,0));
	    panic();
	    break;

	case msg_label_hvm_fault_start ... msg_label_hvm_fault_end:
	    ASSERT (tid == vcpu.main_ltid || tid == vcpu.main_gtid);
	    vcpu.main_info.mr_save.store(tag);
	    dprintf(debug_hvm_fault, "main vfault %x, ip %x\n", 
		    L4_Label(tag), vcpu.main_info.mr_save.get_exc_ip());
	    vcpu.main_info.mr_save.dump(debug_hvm_fault + 1);
	    // process message
	    if( !backend_handle_vfault_message() ) 
	    {
		printf( "Unhandled vfault message %x from %t\n", tag.raw, tid);
		tid = L4_nilthread;
	    }
	    dprintf(debug_hvm_fault, "hvm vfault reply %t\n", tid);
	    vcpu_info->mr_save.load();
	    DEBUGGER_ENTER("REPLY");
	    break;
	default:
	    printf( "Unhandled message tag %x from %t\n", tag.raw, tid);
	    DEBUGGER_ENTER("monitor: unhandled message");
	    tid = L4_nilthread;
	}
    }
}

