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
#include INC_WEDGE(vm.h)
#include INC_WEDGE(irq.h)


static const bool debug_idle=0;
static const bool debug_user=0;
static const bool debug_user_pfault=0;
static const bool debug_user_syscall=0;
static const bool debug_user_startup=0;
static const bool debug_signal=1;

word_t user_vaddr_end = 0x80000000;
void backend_interruptible_idle( burn_redirect_frame_t *redirect_frame )
{
    vcpu_t &vcpu = get_vcpu();

    if( !vcpu.cpu.interrupts_enabled() )
	PANIC( "Idle with interrupts disabled!" );
    if( redirect_frame->do_redirect() )
	return;	// We delivered an interrupt, so cancel the idle.

    L4_ThreadId_t tid = L4_nilthread;
    L4_MsgTag_t tag;
    L4_Word_t err = 0;

    if( debug_idle )
	con << "Entering idle\n";

    // Disable preemption to avoid race conditions with virtual, 
    // asynchronous interrupts.  TODO: make this work with interrupts of
    // physical devices too (i.e., lower their priorities).
    L4_DisablePreemption();
    vcpu.dispatch_ipc_enter();
    tag = L4_Wait( &tid );
    vcpu.dispatch_ipc_exit();
    L4_EnablePreemption();
    if( L4_PreemptionPending() )
	L4_Yield();

    if( L4_IpcFailed(tag) )
	err = L4_ErrorCode();

#warning Pistachio doesn't return local ID's!!
    if( L4_IpcSucceeded(tag) ) {
	if( msg_is_vector(tag) /* && L4_IsLocalId(tid) */ ) {
	    L4_Word_t vector;
	    msg_vector_extract( &vector );

	    ASSERT( !redirect_frame->is_redirect() );
	    redirect_frame->do_redirect( vector );
	}
	else if( msg_is_virq(tag) ) {
	    L4_Word_t irq;
	    msg_virq_extract( &irq );

	    get_intlogic().raise_irq(irq);
	    ASSERT( !redirect_frame->is_redirect() );
	    redirect_frame->do_redirect();
	}
	else {
	    con << "Unexpected IPC in idle loop, from TID " << tid
		<< ", tag " << (void *)tag.raw << '\n';
	}
    }
    else {
	con << "IPC failure in idle loop.  L4 error code " << err << '\n';
    }
}    

NORETURN void backend_activate_user( iret_handler_frame_t *iret_emul_frame )
{
    vcpu_t &vcpu = get_vcpu();
    bool complete;

    if( debug_user )
	con << "Request to enter user"
	    << ", to ip " << (void *)iret_emul_frame->iret.ip
	    << ", to sp " << (void *)iret_emul_frame->iret.sp << '\n';

    // Protect our message registers from preemption.
    vcpu.cpu.disable_interrupts();
    iret_emul_frame->iret.flags.x.fields.fi = 0;

    // Update emulated CPU state.
    vcpu.cpu.cs = iret_emul_frame->iret.cs;
    vcpu.cpu.flags = iret_emul_frame->iret.flags;
    vcpu.cpu.ss = iret_emul_frame->iret.ss;

    L4_ThreadId_t reply_tid = L4_nilthread;

    thread_info_t *thread_info = (thread_info_t *)afterburn_thread_get_handle();
    if( EXPECT_FALSE(!thread_info || 
		    (thread_info->ti->get_page_dir() != vcpu.cpu.cr3.get_pdir_addr())) )
    {
	if( thread_info ) {
	    // The thread switched to a new address space.  Delete the
	    // old thread.  In Unix, for example, this would be a vfork().
	    delete_user_thread( thread_info );
	}

	// We are starting a new thread, so the reply message is the
	// thread startup message.
	thread_info = allocate_user_thread();
	reply_tid = thread_info->get_tid();
	thread_info->state = thread_info_t::state_user;
	// Prepare the startup IPC
	
	thread_info->mr_save.load_startup_reply(iret_emul_frame);
	
	if( debug_user_startup )
	    con << "New thread start, TID " << thread_info->get_tid() << '\n';

    }
    else if( thread_info->state == thread_info_t::state_except_reply )
    {
	reply_tid = thread_info->get_tid();
	thread_info->state = thread_info_t::state_user;
	
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
    else if( thread_info->state == thread_info_t::state_pending )
    {
	// Pre-existing message.  Figure out the target thread's pending state.
	// We discard the iret user-state because it is supposed to be bogus
	// (we haven't given the kernel good state, and via the signal hook,
	// asked the guest kernel to cancel signal delivery).
	reply_tid = thread_info->get_tid();
	switch( L4_Label(thread_info->mr_save.get_msg_tag()) )
	{
	    case msg_label_pfault_start ... msg_label_pfault_end:
		// Reply to fault message.
		thread_info->state = thread_info_t::state_pending;
		complete = handle_user_pagefault( vcpu, thread_info, reply_tid );
		ASSERT( complete );
		break;

	    case msg_label_exception:
		thread_info->state = thread_info_t::state_except_reply;
		backend_handle_user_exception( thread_info );
		panic();
		break;

	    default:
		reply_tid = L4_nilthread;	// No message to user.
		break;
	}
	// Clear the pre-existing message to prevent replay.
	thread_info->mr_save.set_msg_tag((L4_MsgTag_t) {raw : 0});
	thread_info->state = thread_info_t::state_user;
    }
    else
    {
	L4_KDB_Enter("VMEXT BUG\n");
	// No pending message to answer.  Thus the app is already at
	// L4 user, with no expectation of a message from us.
	reply_tid = L4_nilthread;
	if( iret_emul_frame->iret.ip != 0 )
	    con << "Attempted signal delivery during async interrupt.\n";
    }

    L4_ThreadId_t current_tid = thread_info->get_tid(), from_tid;

    for(;;)
    {
	// Send and wait for message.
	// Disable preemption to avoid race conditions with virtual, 
	// asynchronous interrupts.  TODO: make this work with interrupts of
	// physical devices too (i.e., lower their priorities).
	vcpu.cpu.restore_interrupts( true );
	L4_MsgTag_t tag = L4_ReplyWait( reply_tid, &from_tid );
	vcpu.cpu.disable_interrupts();
	vcpu.dispatch_ipc_exit();
	L4_EnablePreemption();
	if( L4_PreemptionPending() )
	    L4_Yield();

	reply_tid = L4_nilthread;

	if( L4_IpcFailed(tag) ) {
	    con << "Dispatch IPC error.\n";
	    continue;
	}

	switch( L4_Label(tag) )
	{
	    case msg_label_pfault_start ... msg_label_pfault_end:
		ASSERT(from_tid == current_tid);
		thread_info->state = thread_info_t::state_pending;
		thread_info->mr_save.set_msg_tag(tag);
		ASSERT( !vcpu.cpu.interrupts_enabled() );
		thread_info->mr_save.store_mrs(tag);
		complete = handle_user_pagefault( vcpu, thread_info, from_tid );
		ASSERT(complete);
		reply_tid = current_tid;
		thread_info->state = thread_info_t::state_user;
		continue;
		
	    case msg_label_exception:
		ASSERT(from_tid == current_tid);
		thread_info->state = thread_info_t::state_except_reply;
		thread_info->mr_save.store_mrs(tag);
		backend_handle_user_exception( thread_info );
		panic();
		continue;
	    case msg_label_preemption:
		thread_info->state = thread_info_t::state_except_reply;
		thread_info->mr_save.store_mrs(tag);
		con << "preemption message "
		    << "from " << from_tid 
		    << "time " << (L4_Word_t) thread_info->mr_save.get_preempt_time()
		    << "\n";
		continue;
		
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
    thread_info_t *thread_info = (thread_info_t *)handle;
    if( EXPECT_FALSE(!thread_info) )
	return 0;

    if( thread_info->state != thread_info_t::state_except_reply ) {
	if( debug_signal )
	    con << "Delayed signal delivery.\n";
	return 1;
    }

    return 0;
}

