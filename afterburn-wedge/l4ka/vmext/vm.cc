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
#include INC_WEDGE(vm.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(hthread.h)
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

bool vm_t::init(word_t ip, word_t sp)
{
    
    // find first elf file among the modules, assume that it is the kernel
    // find first non elf file among the modules assume that it is a ramdisk
    guest_kernel_module = NULL;
    ramdisk_start = NULL;
    ramdisk_size = 0;
    entry_ip = ip;
    entry_sp = sp;
    
#if defined(CONFIG_WEDGE_STATIC)
    cmdline = resourcemon_shared.cmdline;
#else
    cmdline = resourcemon_shared.modules[0].cmdline;
#endif

    return true;
}


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

    if( debug_idle)
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
    L4_MapItem_t map_item;
    L4_ThreadId_t reply_tid = L4_nilthread;
    task_info_t *task_info = NULL;
    
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


#if defined(CONFIG_VSMP)
    thread_mgmt_lock.lock();
#endif    
    /* Release old task */
    if (vcpu.user_info && vcpu.user_info->ti)
	vcpu.user_info->ti->dec_ref_count();    
   
    /* Find new task */
    task_info = get_task_manager().find_by_page_dir(vcpu.cpu.cr3.get_pdir_addr(), true);
    ASSERT(task_info);
    
    /* Get vcpu thread */
    vcpu.user_info = task_info->get_vcpu_thread(vcpu.cpu_id, true);
    ASSERT(vcpu.user_info);
    ASSERT(vcpu.user_info->ti);

    vcpu.user_info->ti->inc_ref_count();    
    afterburn_thread_assign_handle( vcpu.user_info );

#if defined(CONFIG_VSMP)
    thread_mgmt_lock.unlock();
#endif
    
    reply_tid = vcpu.user_info->get_tid();
	
    switch (vcpu.user_info->state)
    {
	case thread_state_startup:
	{
	    // Prepare the startup IPC
	    vcpu.user_info->mr_save.load_startup_reply(iret_emul_frame);
	}
	break;
	case thread_state_exception:
	{
	    if (debug_user_syscall)
	    {
		if( vcpu.user_info->mr_save.get(OFS_MR_SAVE_EAX) == 3 
			/* && vcpu.user_info->mr_save.get(OFS_MR_SAVE_ECX) > 0x7f000000*/ )
		    con << "< read " << vcpu.user_info->mr_save.get(OFS_MR_SAVE_EBX);
		else if( vcpu.user_info->mr_save.get(OFS_MR_SAVE_EAX) == 5 )
		    con << "< open " << (void *)vcpu.user_info->mr_save.get(OFS_MR_SAVE_EBX);
		else if( vcpu.user_info->mr_save.get(OFS_MR_SAVE_EAX) == 90 ) 
		    con << "< mmap ";
		else if( vcpu.user_info->mr_save.get(OFS_MR_SAVE_EAX) == 192 )
		    con << "< mmap2 ";
		else
		    con << "< syscall " << (void *)vcpu.user_info->mr_save.get(OFS_MR_SAVE_EAX);
		con << "\n";
		vcpu.user_info->mr_save.dump();
	    }
	    // Prepare the reply to the exception
	    vcpu.user_info->mr_save.load_exception_reply(iret_emul_frame);
	}
	break;
	case thread_state_pfault:
	{
	    /* 
	     * jsXXX: maybe we can coalesce both cases (exception and pfault)
	     * and just load the regs accordingly
	     */
	    ASSERT( L4_Label(vcpu.user_info->mr_save.get_msg_tag()) >= msg_label_pfault_start);
	    ASSERT( L4_Label(vcpu.user_info->mr_save.get_msg_tag()) <= msg_label_pfault_end);
	    backend_handle_user_pagefault(vcpu.user_info, reply_tid, map_item );
	    vcpu.user_info->mr_save.load_pfault_reply(map_item, iret_emul_frame);
			
	    if (debug_user_pfault)
	    {
		con << "pfault "
		    << "from " << reply_tid
		    << "\n";
		vcpu.user_info->mr_save.dump();
	    }

	
	}
	break;
	case thread_state_preemption:
	{
	    // Prepare the reply to the exception
	    vcpu.user_info->mr_save.load_preemption_reply(iret_emul_frame);

	    if (debug_user_preemption)
	    {
		con << "> preemption "
		    << "from " << reply_tid 	
		    << '\n';
		vcpu.user_info->mr_save.dump();
	    }

	}
	break;
	default:
	{
	    con << "VMEXT Bug invalid user level thread state\n";
	    DEBUGGER_ENTER();
	}
    }
    

    L4_ThreadId_t current_tid = vcpu.user_info->get_tid(), from_tid;

    for(;;)
    {
	// Load MRs
	//L4_Word_t untyped = 0;
#if defined(CONFIG_VSMP)
	thread_mgmt_lock.lock();
	vcpu.user_info->ti->commit_helper();
	thread_mgmt_lock.unlock();
#endif
	vcpu.user_info->mr_save.load();
	L4_MsgTag_t tag;
	
	vcpu.dispatch_ipc_enter();
	vcpu.cpu.restore_interrupts( true );
	tag = L4_ReplyWait( reply_tid, &from_tid );
	vcpu.cpu.disable_interrupts();
	vcpu.dispatch_ipc_exit();
	    
	    
	if( L4_IpcFailed(tag) ) {
	    L4_KDB_Enter("VMEXT Dispatch IPC error");
	    con << "VMEXT Dispatch IPC error to " << reply_tid << ".\n";
	    reply_tid = L4_nilthread;
	    continue;
	}
	reply_tid = L4_nilthread;


	switch( L4_Label(tag) )
	{
	    case msg_label_pfault_start ... msg_label_pfault_end:
		ASSERT(from_tid == current_tid);
		ASSERT( !vcpu.cpu.interrupts_enabled() );
		vcpu.user_info->state = thread_state_pfault;
		vcpu.user_info->mr_save.store_mrs(tag);
		backend_handle_user_pagefault( vcpu.user_info, from_tid, map_item );
		vcpu.user_info->mr_save.load_pfault_reply(map_item);
		reply_tid = current_tid;
		break;
		
	    case msg_label_exception:
		ASSERT(from_tid == current_tid);
		vcpu.user_info->state = thread_state_exception;
		vcpu.user_info->mr_save.store_mrs(tag);
		backend_handle_user_exception( vcpu.user_info );
		panic();
		break;
		
	    case msg_label_preemption:
	    {
		vcpu.user_info->state = thread_state_preemption;
		vcpu.user_info->mr_save.store_mrs(tag);
		backend_handle_user_preemption( vcpu.user_info );
		vcpu.user_info->mr_save.load_preemption_reply();
		vcpu.user_info->mr_save.load();
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
		con << "VIRQ message from TID " << from_tid
		    << ", tag " << (void *)tag.raw << '\n';
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

void backend_free_pgd_hook( pgent_t *pgdir )
{
    cpu_t &cpu = get_cpu();
    bool saved_int_state = cpu.disable_interrupts();
#if defined(CONFIG_VSMP)
    thread_mgmt_lock.lock();
#endif
    task_info_t *task_info = get_task_manager().find_by_page_dir((L4_Word_t) pgdir);
    if (task_info)
	task_info->schedule_free();
    else 
	con << "Task disappeared already pgd " << pgdir << "\n";
#if defined(CONFIG_VSMP)
    thread_mgmt_lock.unlock();
#endif
    cpu.restore_interrupts( saved_int_state );
}

void backend_exit_hook( void *handle )
{
}

int backend_signal_hook( void *handle )
{
    // Return 0 to permit signal delivery.
    return 0;
}

