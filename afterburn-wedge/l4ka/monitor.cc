/*********************************************************************
 *
 * Copyright (C) 2005,  Unversity of Karlsruhe
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

#include INC_ARCH(ia32.h)
#include INC_ARCH(page.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(monitor.h)
#include INC_WEDGE(l4thread.h)
#include INC_WEDGE(irq.h)


void monitor_loop( vcpu_t & vcpu, vcpu_t &activator )
{
    L4_ThreadId_t from = L4_nilthread;
    L4_ThreadId_t to = L4_nilthread;
    L4_Word_t timeouts = default_timeouts;
    thread_info_t *vcpu_info;
    L4_Error_t errcode;
    L4_MsgTag_t tag;
    
    // Set our thread's exception handler. 
    L4_Set_ExceptionHandler( L4_Pager());

    vcpu.main_info.mr_save.load();
    to = vcpu.main_gtid;

    for (;;) 
    {
	L4_Accept(L4_UntypedWordsAcceptor);
	tag = L4_Ipc( to, L4_anythread, timeouts, &from);
	timeouts = default_timeouts;
	
	if ( L4_IpcFailed(tag) )
	{
	    errcode = L4_ErrorCode();
	    	    		
#if defined(CONFIG_L4KA_VMEXT)
	    if ((L4_ErrorCode() & 0xf) == 3)
	    {
		/* 
		 * We get a receive timeout, when the current thread hasn't send a
		 * preemption reply, (e.g., because it's waiting for some other thread's
		 * service and we didn't get an IDLE IPC)
		 */
		dprintf(debug_preemption, "monitor receive timeout to %t from %t error %d\n", to, from, errcode);
		to = vcpu.get_hwirq_tid();
		vcpu.irq_info.mr_save.load_yield_msg(L4_nilthread, false);
		vcpu.irq_info.mr_save.load();
		timeouts = vtimer_timeouts;
	    }
	    else
#endif
	    {
		/* 
		 * We get a send timeout, when trying to send to a non-preempted
		 * thread (e.g., if thread is waiting for roottask service or 
		 * polling
		 */
		printf("monitor send timeout to %t from %t error %d\n", to, from, errcode);
		DEBUGGER_ENTER("monitor bug");
		to = L4_nilthread;
	    }
	    continue;
	}

	switch( L4_Label(tag) )
	{
	case msg_label_migration:
	{
	    printf( "received migration request\n");
	    // reply to resourcemonitor
	    // and get moved over to the new host
	    to = from;
	}
	break;
	case msg_label_thread_create:
	{
	    vcpu_t *tvcpu;
	    L4_Word_t stack_bottom;
	    L4_Word_t stack_size;
	    L4_Word_t prio;
	    l4thread_func_t start_func;
	    L4_ThreadId_t pager_tid;
	    void *start_param;
	    void *tlocal_data;
	    L4_Word_t tlocal_size;

	    msg_thread_create_extract((void**) &tvcpu, &stack_bottom, &stack_size, &prio, 
				      (void *) &start_func, &pager_tid, &start_param, &tlocal_data, &tlocal_size); 

	    l4thread_t *l4thread = get_l4thread_manager()->create_thread(tvcpu, stack_bottom, stack_size, prio, 
								      start_func, pager_tid, start_param, 
								      tlocal_data, tlocal_size);
		
	    msg_thread_create_done_build(l4thread);
	    to = from;
	}
	break;
	case msg_label_pfault_start ... msg_label_pfault_end:
	{
	    vcpu_info = backend_handle_pagefault(tag, from);
	    ASSERT(vcpu_info);
	    vcpu_info->mr_save.load();
	    to = from;
	}
	break;
	case msg_label_exception:
	{
	    ASSERT (from == vcpu.main_ltid || from == vcpu.main_gtid);
	    vcpu.main_info.mr_save.store(tag);
		
	    if (vcpu.main_info.mr_save.get_exc_number() == X86_EXC_NOMATH_COPROC)	
	    {
		printf( "FPU main exception, ip %x\n", vcpu.main_info.mr_save.get_exc_ip());
		vcpu.main_info.mr_save.load_exception_reply(true, NULL);
		vcpu.main_info.mr_save.load();
		to = vcpu.main_gtid;
	    }
	    else
	    {
		printf( "Unhandled main exception %d, ip %x no %\n", 
			vcpu.main_info.mr_save.get_exc_number(),
			vcpu.main_info.mr_save.get_exc_ip());
		panic();
	    }
	}
	break;
#if defined(CONFIG_L4KA_VMEXT)
	case msg_label_preemption ... msg_label_preemption_reply:
	    backend_handle_preemption(tag, from, to, timeouts);
	    break;
	case msg_label_virq:
	case msg_label_hwirq:
	case msg_label_ipi:
	    backend_handle_hwirq(tag, from, to, timeouts);
	    break;
	break;
#endif /* defined(CONFIG_L4KA_VMEXT) */	
#if defined(CONFIG_L4KA_HVM)
	case msg_label_hvm_fault_start ... msg_label_hvm_fault_end:
	    ASSERT (from == vcpu.main_ltid || from == vcpu.main_gtid);
	    vcpu.main_info.mr_save.store(tag);
	    
	    // process message
	    if( !backend_handle_vfault() ) 
	    {
		to = L4_nilthread;
		vcpu.dispatch_ipc_enter();
	    }
	    else
	    {
		to = from;
		vcpu.main_info.mr_save.load();
	    }
	    break;
#endif
	default:
	    printf( "Unhandled message %x from TID %t\n", tag.raw, from);
	    DEBUGGER_ENTER("monitor: unhandled message");


	}
    }
}

