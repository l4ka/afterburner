/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4-common/vm.cc
 * Description:   The L4 state machine for implementing the idle loop,
 *                and the concept of "returning to user".  When returning
 *                to user, enters a dispatch loop, for handling L4 messages.
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

#include <l4/schedule.h>
#include <l4/ipc.h>

#include <bind.h>
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(setup.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(user.h)
#include INC_WEDGE(irq.h)


static const bool debug_idle=0;
static const bool debug_user=0;
static const bool debug_user_pfault=0;
static const bool debug_user_syscall=0;
static const bool debug_signal=1;
static const bool debug_user_preemption=0;

word_t user_vaddr_end = 0x80000000;
void backend_interruptible_idle( burn_redirect_frame_t *redirect_frame )
{
    vcpu_t &vcpu = get_vcpu();
    
    if( !vcpu.cpu.interrupts_enabled() )
	PANIC( "Idle with interrupts disabled!" );
    
    if( redirect_frame->do_redirect() )
    {
	//con << "Idle canceled\n";
	return;	// We delivered an interrupt, so cancel the idle.
    }

    if( debug_idle )
	con << "Entering idle\n";
    
    /* Yield will synthesize a preemption IPC */
    vcpu.idle_enter(redirect_frame);
    L4_ThreadSwitch(vcpu.monitor_gtid);
    if (!redirect_frame->is_redirect())
	L4_KDB_Enter("Redirect");
    
    ASSERT(redirect_frame->is_redirect());
    if( debug_idle )
	con << "Idle returns";
}    

NORETURN void backend_activate_user( iret_handler_frame_t *iret_emul_frame )
{
    vcpu_t &vcpu = get_vcpu();
    bool complete;

    if( debug_user )
	con << "Request to enter user"
	    << ", ip " << (void *)iret_emul_frame->iret.ip
	    << ", sp " << (void *)iret_emul_frame->iret.sp 
	    << ", flags " << (void *)iret_emul_frame->iret.flags.x.raw
	    << '\n';

    // Protect our message registers from preemption.
    vcpu.cpu.disable_interrupts();
    iret_emul_frame->iret.flags.x.fields.fi = 0;

    // Update emulated CPU state.
    vcpu.cpu.cs = iret_emul_frame->iret.cs;
    vcpu.cpu.flags = iret_emul_frame->iret.flags;
    vcpu.cpu.ss = iret_emul_frame->iret.ss;

    L4_ThreadId_t reply_tid = L4_nilthread;
 
    thread_info_t *thread_info;
    task_info_t *task_info = 
	task_manager_t::get_task_manager().find_by_page_dir(vcpu.cpu.cr3.get_pdir_addr());
    
    if (!task_info || task_info->get_vcpu_thread(vcpu.cpu_id) == NULL)
    {
	// We are starting a new thread, so the reply message is the
	// thread startup message.
	thread_info = allocate_user_thread(task_info);
	afterburn_thread_assign_handle( thread_info );
	task_info = thread_info->ti;
	task_info->set_vcpu_thread(vcpu.cpu_id, thread_info);
	reply_tid = thread_info->get_tid();
	// Prepare the startup IPC
	thread_info->mr_save.load_startup_reply(iret_emul_frame);
	
    }
    else 
    {
	thread_info = task_info->get_vcpu_thread(vcpu.cpu_id);
	ASSERT(thread_info);
	reply_tid = thread_info->get_tid();
	
	switch (thread_info->state)
	{
	    case thread_state_exception:
	    {
		if (debug_user_syscall)
		{
		    if( thread_info->mr_save.get(OFS_MR_SAVE_EAX) == 3 
			    /* && thread_info->mr_save.get(OFS_MR_SAVE_ECX) > 0x7f000000*/ )
			con << "< read " << thread_info->mr_save.get(OFS_MR_SAVE_EBX);
		    else if( thread_info->mr_save.get(OFS_MR_SAVE_EAX) == 5 )
			con << "< open " << (void *)thread_info->mr_save.get(OFS_MR_SAVE_EBX);
		    else if( thread_info->mr_save.get(OFS_MR_SAVE_EAX) == 90 ) 
			con << "< mmap ";
		    else if( thread_info->mr_save.get(OFS_MR_SAVE_EAX) == 192 )
			con << "< mmap2 ";
		    else
			con << "< syscall " << (void *)thread_info->mr_save.get(OFS_MR_SAVE_EAX);
	    
		    con << ", eax " << (void *) iret_emul_frame->frame.x.fields.eax
			<< ", ebx " << (void *) iret_emul_frame->frame.x.fields.ebx
			<< ", ecx " << (void *) iret_emul_frame->frame.x.fields.ecx
			<< ", edx " << (void *) iret_emul_frame->frame.x.fields.edx
			<< '\n';
		}
		// Prepare the reply to the exception
		thread_info->mr_save.load_exception_reply(iret_emul_frame);
	    }
	    break;
	    case thread_state_pfault:
	    {
		/* 
		 * jsXXX: maybe we can coalesce both cases (exception and pfault)
		 * and just load the regs accordingly
		 */
		if (L4_Label(thread_info->mr_save.get_msg_tag()) < msg_label_pfault_start 
			||L4_Label(thread_info->mr_save.get_msg_tag()) > msg_label_pfault_end)
		{
		    con << "ti " << (void *) thread_info << " tid " << thread_info->get_tid() << "\n";
		    con << "bug    "
			<< ", eip " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EIP)	
			<< ", efl " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EFLAGS)
			<< ", edi " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EDI)
			<< ", esi " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_ESI)
			<< ", ebp " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EBP)
			<< ", esp " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_ESP)
			<< ", ebx " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EBX)
			<< ", edx " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EDX)
			<< ", ecx " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_ECX)
			<< ", eax " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EAX)
			<< "\n";
		}

		ASSERT( L4_Label(thread_info->mr_save.get_msg_tag()) >= msg_label_pfault_start);
		ASSERT( L4_Label(thread_info->mr_save.get_msg_tag()) <= msg_label_pfault_end);
		complete = handle_user_pagefault( vcpu, thread_info, reply_tid );
		ASSERT( complete );
	
	    }
	    break;
	    case thread_state_preemption:
	    {
		// Prepare the reply to the exception
		thread_info->mr_save.load_preemption_reply(iret_emul_frame);

		if (debug_user_preemption)
		    con << "> preemption "
			<< "from " << reply_tid 
			<< ", eip " << (void *)thread_info->mr_save.get(OFS_MR_SAVE_EIP)
			<< ", eax " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EAX)
			<< ", ebx " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EBX)
			<< ", ecx " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_ECX)
			<< ", edx " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EDX)
			<< '\n';

	    }
	    break;
	    default:
	    {
		con << "VMEXT Bug invalid user level thread state\n";
		DEBUGGER_ENTER();
	    }
	}
    }

    L4_ThreadId_t current_tid = thread_info->get_tid(), from_tid;

    for(;;)
    {
#if defined(CONFIG_VSMP)
	// Any helper tasks? 
	L4_Word_t offset = thread_info->ti->commit_helper(true);
	// Load MRs
	thread_info->mr_save.load_mrs(offset);
	
	if (debug_helper && offset)
	{
	    con << "helper "
		<< ", eip " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EIP)	
		<< ", efl " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EFLAGS)
		<< ", edi " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EDI)
		<< ", esi " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_ESI)
		<< ", ebp " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EBP)
		<< ", esp " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_ESP)
		<< ", ebx " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EBX)
		<< ", edx " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EDX)
		<< ", ecx " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_ECX)
		<< ", eax " << (void *) thread_info->mr_save.get(OFS_MR_SAVE_EAX)
		<< "\n";
	}
#else
	// Load MRs
	thread_info->mr_save.load_mrs();
#endif	

	L4_MsgTag_t tag;
	
	vcpu.dispatch_ipc_enter();
	vcpu.cpu.restore_interrupts( true );
	tag = L4_ReplyWait( reply_tid, &from_tid );
	vcpu.cpu.disable_interrupts();
	vcpu.dispatch_ipc_exit();
	    
	    
	reply_tid = L4_nilthread;

	if( L4_IpcFailed(tag) ) {
	    con << "VMEXT Dispatch IPC error.\n";
	    DEBUGGER_ENTER();
	    continue;
	}
	
	switch( L4_Label(tag) )
	{
	    case msg_label_pfault_start ... msg_label_pfault_end:
		ASSERT(from_tid == current_tid);
		ASSERT( !vcpu.cpu.interrupts_enabled() );
		thread_info->state = thread_state_pfault;
		thread_info->mr_save.store_mrs(tag);
		complete = handle_user_pagefault( vcpu, thread_info, from_tid );
		if (!complete)
		{
		    con << "vnext bug " 
			<< " pp " << (void *) thread_info->ti->get_page_dir()
			<< " cr " << (void *) vcpu.cpu.cr3.get_pdir_addr()
			<< " td " << thread_info->get_tid()
			<< " ti " << thread_info
			<< "\n"; 
		    DEBUGGER_ENTER(0);
		}
		ASSERT(complete);
		reply_tid = current_tid;
		break;
		
	    case msg_label_exception:
		ASSERT(from_tid == current_tid);
		thread_info->state = thread_state_exception;
		thread_info->mr_save.store_mrs(tag);
		backend_handle_user_exception( thread_info );
		panic();
		break;
		
	    case msg_label_preemption:
	    {
		thread_info->state = thread_state_preemption;
		thread_info->mr_save.store_mrs(tag);
#if defined(CONFIG_VSMP)
		if (!is_helper_addr(thread_info->mr_save.get_preempt_ip()))
		    backend_handle_user_preemption( thread_info );
#else
		backend_handle_user_preemption( thread_info );
#endif
		
		thread_info->mr_save.load_preemption_reply();
		thread_info->mr_save.load_mrs();
		reply_tid = current_tid;
		break;
	    }
	    case msg_label_vector: 
	    {
		L4_Word_t vector;
		msg_vector_extract( &vector );
		backend_handle_user_vector( vector );
		panic();
		break;
	    }

	    case msg_label_virq: {
		L4_Word_t msg_irq;
		word_t irq, vector;
		msg_virq_extract( &msg_irq );
		get_intlogic().raise_irq( msg_irq );
		if( get_intlogic().pending_vector(vector, irq) )
		    backend_handle_user_vector( vector );
		break;
	    }

	    default:
		con << "Unexpected message from TID " << from_tid
		    << ", tag " << (void *)tag.raw << '\n';
		L4_KDB_Enter("unknown message");
		break;
	}

    }

    panic();
}

void backend_exit_hook( void *handle )
{
    cpu_t &cpu = get_cpu();
    bool saved_int_state = cpu.disable_interrupts();
    delete_user_thread( (thread_info_t *)handle );
    cpu.restore_interrupts( saved_int_state );
}

int backend_signal_hook( void *handle )
// Return 1 to cancel signal delivery.
// Return 0 to permit signal delivery.
{
    return 0;
}

