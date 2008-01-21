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
#include <console.h>
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include <debug.h>
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(user.h)
#include INC_WEDGE(irq.h)



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
    {
	printf( "vcpu %d\n", &get_vcpu());
	PANIC( "Idle with interrupts disabled!" );
    }
    
    if( redirect_frame->do_redirect() )
    {
	//printf( "Idle canceled\n");
	return;	// We delivered an interrupt, so cancel the idle.
    }

    dprintf(debug_idle, "Entering idle\n");
    
    /* Yield will synthesize a preemption IPC */
    vcpu.idle_enter(redirect_frame);
    L4_ThreadSwitch(vcpu.monitor_gtid);
    if (!redirect_frame->is_redirect())
	DEBUGGER_ENTER("Redirect");
    
    ASSERT(redirect_frame->is_redirect());
    dprintf(debug_idle, "Idle returns");
}    

NORETURN void backend_activate_user( iret_handler_frame_t *iret_emul_frame )
{
    vcpu_t &vcpu = get_vcpu();
    L4_MapItem_t map_item;
    L4_ThreadId_t reply_tid = L4_nilthread;
    task_info_t *task_info = NULL;
    intlogic_t &intlogic = get_intlogic();
    word_t irq, vector; 
    L4_MsgTag_t tag;
   
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
    
    dprintf(debug_iret, "Request to enter user IP %x SP %x FLAGS %x TID %t\n", 
	    iret_emul_frame->iret.ip, iret_emul_frame->iret.sp, 
	    iret_emul_frame->iret.flags.x.raw, vcpu.user_info->get_tid());

    switch (vcpu.user_info->state)
    {
    case thread_state_startup:
    {
	// Prepare the startup IPC
	dprintf(debug_task, "> startup %t", reply_tid);
	vcpu.user_info->mr_save.load_startup_reply(iret_emul_frame);
    }
    break;
    case thread_state_exception:
    {
	if( vcpu.user_info->mr_save.get(OFS_MR_SAVE_EAX) == 3 )
	    dprintf(debug_syscall, "> read %x",  vcpu.user_info->mr_save.get(OFS_MR_SAVE_EBX));
	else if( vcpu.user_info->mr_save.get(OFS_MR_SAVE_EAX) == 4 )
	    dprintf(debug_syscall, "> write %x",  vcpu.user_info->mr_save.get(OFS_MR_SAVE_EBX));
	else if( vcpu.user_info->mr_save.get(OFS_MR_SAVE_EAX) == 5 )
	    dprintf(debug_syscall, "> open %x",  (void *)vcpu.user_info->mr_save.get(OFS_MR_SAVE_EBX));
	else if( vcpu.user_info->mr_save.get(OFS_MR_SAVE_EAX) == 90 ) 
	{
	    word_t *args = (word_t *)vcpu.user_info->mr_save.get(OFS_MR_SAVE_EBX);
	    dprintf(debug_syscall, "> mmap fd %x, len %d, addr %x offset %x\n",
		    args[4], args[1], args[0], args[5]);
	}
	else if( vcpu.user_info->mr_save.get(OFS_MR_SAVE_EAX) == 91 )
	    dprintf(debug_syscall, "> munmap ");
	else  if( vcpu.user_info->mr_save.get(OFS_MR_SAVE_EAX) == 2 ) 
	    dprintf(debug_syscall, "> fork ");
	else if( vcpu.user_info->mr_save.get(OFS_MR_SAVE_EAX) == 120 )
	    dprintf(debug_syscall, "> clone\n");
	else
	    dprintf(debug_syscall, "> syscall %x", vcpu.user_info->mr_save.get(OFS_MR_SAVE_EAX));
	
	// Prepare the reply to the exception
	vcpu.user_info->mr_save.load_exception_reply(false, iret_emul_frame);
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
			
	dprintf(debug_pfault, "> pfault from %t", reply_tid);

    }
    break;
    case thread_state_preemption:
    {
	// Prepare the reply to the exception
	vcpu.user_info->mr_save.load_preemption_reply(true, iret_emul_frame);
	dprintf(debug_preemption, "> preemption from %t", reply_tid);
    }
    case thread_state_activated:
    {	
	dprintf(debug_preemption, "> preemption during activation from %t", reply_tid);
	vcpu.user_info->mr_save.load_activation_reply(iret_emul_frame);
    }
    break;
    default:
    {
	printf( "VMEXT Bug invalid user level thread state\n");
	DEBUGGER_ENTER("BUG");
    }
    }
    

    L4_ThreadId_t current_tid = vcpu.user_info->get_tid(), from_tid;

    for(;;)
    {
	// Load MRs
#if defined(CONFIG_VSMP)
	thread_mgmt_lock.lock();
	vcpu.user_info->ti->commit_helper();
	thread_mgmt_lock.unlock();
#endif
	if (intlogic.pending_vector(vector, irq))
	{
	    vcpu.user_info->state = thread_state_activated;
	    backend_handle_user_vector( vcpu.user_info, vector );
	}
	
	vcpu.user_info->mr_save.load();
	
	vcpu.dispatch_ipc_enter();
	vcpu.cpu.restore_interrupts( true );
	L4_Accept(L4_UntypedWordsAcceptor);
	tag = L4_ReplyWait( reply_tid, &from_tid );
	vcpu.cpu.disable_interrupts();
	vcpu.dispatch_ipc_exit();
	    
	    
	if( L4_IpcFailed(tag) ) {
	    DEBUGGER_ENTER("VMEXT Dispatch IPC error");
	    printf( "VMEXT Dispatch IPC error to %t\n", reply_tid);
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
	    vcpu.user_info->mr_save.load_exception_reply(true, NULL);
	    reply_tid = current_tid;
	    break;
		
	case msg_label_preemption:
	{
	    vcpu.user_info->state = thread_state_preemption;
	    vcpu.user_info->mr_save.store_mrs(tag);
	    backend_handle_user_preemption( vcpu.user_info );
	    vcpu.user_info->mr_save.load_preemption_reply(true);
	    reply_tid = current_tid;
	    break;
	}
	case msg_label_vector: 
	{
	    msg_vector_extract( (L4_Word_t *) &vector, (L4_Word_t *) &irq );
	    backend_handle_user_vector( vcpu.user_info, vector );
	    break;
	}

	case msg_label_virq: 
	{
	    L4_Word_t msg_irq;
	    msg_virq_extract( &msg_irq );
	    get_intlogic().raise_irq( msg_irq );
	    if (intlogic.pending_vector(vector, irq))
		backend_handle_user_vector( vcpu.user_info, vector );
	    break;
	}

	default:
	    printf( "Unexpected message from TID %t tag %x\n", from_tid, tag.raw);
	    DEBUGGER_ENTER("unknown message");
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
	printf( "Task disappeared already pgd %x", pgdir);
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

