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
#include <debug.h>
#include <console.h>
#include <l4/schedule.h>
#include <l4/ipc.h>
#include <bind.h>

#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(l4thread.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(monitor.h)

extern void NORETURN deliver_ia32_user_exception( thread_info_t *thread_info, word_t vector, bool error_code=false);

INLINE bool async_safe( word_t ip )
{
    return (ip < CONFIG_WEDGE_VIRT);
}

word_t user_vaddr_end = 0x80000000;

void NORETURN
backend_handle_user_exception( thread_info_t *thread_info, word_t vector )
{
    deliver_ia32_user_exception( thread_info, vector );
}

bool deliver_ia32_exception(cpu_t & cpu, L4_Word_t vector, u32_t error_code, thread_info_t *thread_info)
{
    // - Byte offset from beginning of IDT base is 8*vector.
    // - Compare the offset to the limit value.  The limit is expressed in 
    // bytes, and added to the base to get the address of the last
    // valid byte.
    // - An empty descriptor slot should have its present flag set to 0.

    ASSERT( L4_MyLocalId() == get_vcpu().monitor_ltid );

    if( vector > cpu.idtr.get_total_gates() ) {
	printf( "No IDT entry for vector %x\n", vector);
	return false;
    }

    gate_t *idt = cpu.idtr.get_gate_table();
    gate_t &gate = idt[ vector ];

    ASSERT( gate.is_trap() || gate.is_interrupt() );
    ASSERT( gate.is_present() );
    ASSERT( gate.is_32bit() );

    dprintf(irq_dbg_level(0, vector), "Delivering vector %x handler ip %x\n", vector, gate.get_offset());

    u16_t old_cs = cpu.cs;
    L4_Word_t old_eip, old_esp, old_eflags;
    
    old_esp = thread_info->mrs.get(OFS_MR_SAVE_ESP);
    old_eip = thread_info->mrs.get(OFS_MR_SAVE_EIP); 
    old_eflags = thread_info->mrs.get(OFS_MR_SAVE_EFLAGS); 

    if( !async_safe(old_eip) )
    {
	printf( "interrupting the wedge to handle a fault, ip %x vector %x cr2 %x handler ip %x called from %x",
		old_eip, vector, cpu.cr2, gate.get_offset(), __builtin_return_address(0));
	DEBUGGER_ENTER("BUG");
    }
    
    // Set VCPU flags
    cpu.flags.x.raw = (old_eflags & flags_user_mask) | (cpu.flags.x.raw & ~flags_user_mask);
    
    // Store values on the stack.
    L4_Word_t *esp = (L4_Word_t *) old_esp;
    *(--esp) = cpu.flags.x.raw;
    *(--esp) = old_cs;
    *(--esp) = old_eip;
    *(--esp) = error_code;

    cpu.cs = gate.get_segment_selector();
    cpu.flags.prepare_for_gate( gate );

    thread_info->mrs.set(OFS_MR_SAVE_ESP, (L4_Word_t) esp);
    thread_info->mrs.set(OFS_MR_SAVE_EIP, gate.get_offset());
    
    return true;
}



/*
 * Returns if redirection was necessary
 */
bool backend_async_deliver_irq( intlogic_t &intlogic )
{
    vcpu_t &vcpu = get_vcpu();
    cpu_t &cpu = vcpu.cpu;

    ASSERT( L4_MyLocalId() != vcpu.main_ltid );
    
    word_t vector, irq;

    /* 
     * We are already executing somewhere in the wedge. Unless we're idle (in
     * which case we've set EIP to 0xFFFFFFFF) we don't deliver interrupts
     * directly.
     */

    if( EXPECT_FALSE(!async_safe(vcpu.main_info.mrs.get(OFS_MR_SAVE_EIP))))
	return vcpu.redirect_idle();

    if( !cpu.interrupts_enabled() )
	return false;
    
    if( !intlogic.pending_vector(vector, irq) )
	return false;

  
    ASSERT( vector < cpu.idtr.get_total_gates() );
    gate_t *idt = cpu.idtr.get_gate_table();
    gate_t &gate = idt[ vector ];

    dprintf(irq_dbg_level(irq), "interrupt deliver vector %d handler %x\n", vector, gate.get_offset());
	

    ASSERT( gate.is_trap() || gate.is_interrupt() );
    ASSERT( gate.is_present() );
    ASSERT( gate.is_32bit() );


    L4_Word_t old_esp, old_eip, old_eflags;
    
    old_esp = vcpu.main_info.mrs.get(OFS_MR_SAVE_ESP);
    old_eip = vcpu.main_info.mrs.get(OFS_MR_SAVE_EIP); 
    old_eflags = vcpu.main_info.mrs.get(OFS_MR_SAVE_EFLAGS); 

    cpu.flags.x.raw = (old_eflags & flags_user_mask) | (cpu.flags.x.raw & ~flags_user_mask);

    L4_Word_t *esp = (L4_Word_t *) old_esp;
    *(--esp) = cpu.flags.x.raw;
    *(--esp) = cpu.cs;
    *(--esp) = old_eip;
    
    vcpu.main_info.mrs.set(OFS_MR_SAVE_EIP, gate.get_offset()); 
    vcpu.main_info.mrs.set(OFS_MR_SAVE_ESP, (L4_Word_t) esp); 
    // Side effects are now permitted to the CPU object.
    cpu.flags.prepare_for_gate( gate );
    cpu.cs = gate.get_segment_selector();
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
    
    vcpu.idle_enter(redirect_frame);
    /* Set EIP to 0 to allow async delivery of IRQs */
    vcpu.main_info.mrs.set(OFS_MR_SAVE_EIP, ~0UL);
    vcpu.main_info.mrs.load_yield_msg(vcpu.monitor_gtid, false);
    vcpu.main_info.mrs.load();
    L4_Call(vcpu.monitor_ltid);
    
    ASSERT(redirect_frame->is_redirect());
    dprintf(debug_idle, "idle returns");
}    

INLINE bool is_l4thread_preempted(word_t &l4thread_idx, bool relaxed=true)
{
    vcpu_t &vcpu = get_vcpu();
    for (l4thread_idx=0; l4thread_idx < vcpu_t::max_l4threads; l4thread_idx++)
	if (vcpu.l4thread_info[l4thread_idx].get_tid() != L4_nilthread)
	{	    
	    if  (vcpu.l4thread_info[l4thread_idx].mrs.is_preemption_msg())
		return true;
	    
	    if (relaxed && vcpu.l4thread_info[l4thread_idx].mrs.is_activation_msg())
		return true;
	    
	    if (relaxed && vcpu.l4thread_info[l4thread_idx].mrs.is_yield_msg())
		return true;
	    

	}
    return false;
}

INLINE bool backend_reschedule(L4_MsgTag_t tag, L4_ThreadId_t from, L4_ThreadId_t &to, L4_Word_t &timeouts)
{
    vcpu_t &vcpu = get_vcpu();
    intlogic_t &intlogic = get_intlogic();
    word_t l4thread_idx;
    
    bool cxfer = backend_async_deliver_irq( intlogic );

    if (!vcpu.is_idle())
    {
	dprintf(debug_preemption, "monitor reschedule found runnable main %t tag %x\n", 
		vcpu.main_gtid, vcpu.main_info.mrs.get_msg_tag());
	    
	ASSERT(!vcpu.main_info.mrs.is_blocked_msg());
		
	// We've blocked a l4thread and main is preempted, switch to main
	vcpu.main_info.mrs.load_preemption_reply(cxfer);
	vcpu.main_info.mrs.load();
	to = vcpu.main_gtid;
    }
    else if (is_l4thread_preempted(l4thread_idx))
    {
	dprintf(debug_preemption, "monitor reschedule found runnable l4thread %d %t tag %x\n", 
		l4thread_idx, vcpu.l4thread_info[l4thread_idx].get_tid(),
		vcpu.l4thread_info[l4thread_idx].mrs.get_msg_tag());
	    
	vcpu.l4thread_info[l4thread_idx].mrs.load_preemption_reply(false);
	vcpu.l4thread_info[l4thread_idx].mrs.load();
	to = vcpu.l4thread_info[l4thread_idx].get_tid();
    }
    else
    {
	dprintf(debug_preemption, "monitor reschedule no thread runnable, yield to virq\n");
	/* yield IPC to the  resourcemon's scheduler */
	vcpu.irq_info.mrs.load_yield_msg(L4_nilthread, false);
	vcpu.irq_info.mrs.load();
	timeouts = vtimer_timeouts;
	to = vcpu.get_hwirq_tid();

    }

}

void backend_handle_hwirq(L4_MsgTag_t tag, L4_ThreadId_t from, L4_ThreadId_t &to, L4_Word_t &timeouts)
{
    intlogic_t &intlogic = get_intlogic();

    switch( L4_Label(tag) )
    {
    case msg_label_virq:
    {
	// Virtual interrupt from external source.
	L4_Word_t irq;
	L4_ThreadId_t ack;

	msg_virq_extract( &irq, &ack );
	ASSERT(intlogic.is_virtual_hwirq(irq));
	dbg_irq(irq);
	dprintf(irq_dbg_level(irq), "virtual irq: %d from %t ack %t\n", irq, from, ack);
	intlogic.set_virtual_hwirq_sender(irq, ack);
	intlogic.raise_irq( irq );
	backend_reschedule(tag, from, to, timeouts);
	break;
    }		    
    case msg_label_ipi:
    {
	L4_Word_t vector;
	L4_Word_t src_vcpu_id;		
	msg_ipi_extract( &src_vcpu_id, &vector  );
	dprintf(irq_dbg_level(0, vector), "virtual ipi from VCPU %d vector %d\n", src_vcpu_id, vector);
#if defined(CONFIG_VSMP)
	local_apic_t &lapic = get_lapic();
	lapic.lock();
	lapic.raise_vector(vector, INTLOGIC_INVALID_IRQ);
	lapic.unlock();
#endif		
	msg_done_build();
	to = from;
    }
    break;
    default:
	printf("unhandled hwirq message from %t\n", from);
	DEBUGGER_ENTER("UNIMPLEMENTED");
	break;
    }
}

void backend_handle_preemption(L4_MsgTag_t tag, L4_ThreadId_t from, L4_ThreadId_t &to, L4_Word_t &timeouts)
{
    word_t l4thread_idx;
    intlogic_t &intlogic = get_intlogic();
    L4_ThreadId_t preempter =  L4_nilthread;
    vcpu_t &vcpu = get_vcpu();
    
    switch( L4_Label(tag) )
    {
    case msg_label_preemption:
    {
	if (from == vcpu.main_ltid || from == vcpu.main_gtid)
	{
	    vcpu.main_info.mrs.store();
	    
	    /* Must be due to preemption of other hthread, irqs are handled by
	     * resourcemon 
	     */
	    preempter =  vcpu.main_info.mrs.get_preempter();

	    dprintf(debug_preemption, "main thread sent preemption IPC ip %x, preempter %t tag %x\n", 
		    vcpu.main_info.mrs.get_preempt_ip(), preempter,   
		    vcpu.l4thread_info[l4thread_idx].mrs.get_msg_tag());

	    
	    bool mbt = vcpu.is_vcpu_thread(preempter, l4thread_idx);
	    ASSERT(mbt);
	    
	    vcpu.l4thread_info[l4thread_idx].mrs.set_msg_tag(tag);
	    
	    vcpu.main_info.mrs.load_preemption_reply(false);
	    vcpu.main_info.mrs.load();
	    to = vcpu.main_gtid;		    
	}
	else if (vcpu.is_vcpu_thread(from, l4thread_idx) || vcpu.is_booting_other_vcpu())
	{
	    to = from;
	    preempter =  vcpu.main_info.mrs.get_preempter();

	    if (vcpu.is_booting_other_vcpu())
	    {
		ASSERT(from == get_vcpu(vcpu.get_booted_cpu_id()).monitor_gtid);
		l4thread_idx = 0;
	    }
		
	    vcpu.l4thread_info[l4thread_idx].mrs.store();
		    
	    dprintf(debug_preemption, "vcpu thread %d %t sent preemption IPC preempter %t %t\n",
		    l4thread_idx, from, preempter);
		    
	    /* Reply instantly */
	    vcpu.l4thread_info[l4thread_idx].mrs.load_preemption_reply(false);
	    vcpu.l4thread_info[l4thread_idx].mrs.load();
	}
	else
	{
	    PANIC( "Unhandled preemption by tid %t\n", from);
	}
		    
    }
    break;
    case msg_label_preemption_yield:
    {
	
	ASSERT(from == vcpu.main_ltid || from == vcpu.main_gtid);	
	vcpu.main_info.mrs.store();

	dprintf(debug_preemption, "main thread sent yield IPC dest %t tag %x\n", 
		vcpu.main_info.mrs.get_yield_dest(), vcpu.main_info.mrs.get_msg_tag().raw);
	
	backend_reschedule(tag, from, to, timeouts);
    }
    break;
    case msg_label_preemption_blocked:
    {
	if (from == vcpu.main_ltid || from == vcpu.main_gtid)
	{
	    vcpu.main_info.mrs.store();
	    preempter =  vcpu.main_info.mrs.get_preempter();
	    
	    dprintf(debug_preemption, "main thread %t sent blocked IPC ip %x, preempter %t\n", 
		    from, vcpu.main_info.mrs.get_preempt_ip(), preempter);

	    bool mbt = vcpu.is_vcpu_thread(preempter, l4thread_idx);
	    ASSERT(mbt);
	    
	    DEBUGGER_ENTER("UNIMPLEMENTED MONITOR MAIN IDLE");
	    
	}
	else if (vcpu.is_vcpu_thread(from, l4thread_idx))
	{
	    vcpu.l4thread_info[l4thread_idx].mrs.store();
	    preempter =  vcpu.l4thread_info[l4thread_idx].mrs.get_preempter();
	    dprintf(debug_preemption, "vcpu thread %d %t sent blocked IPC, preempter %t\n", 
		    l4thread_idx, from, preempter);
	    
	    if (preempter == from || preempter == vcpu.main_gtid || 
		vcpu.is_vcpu_thread(preempter, l4thread_idx))
	    {
		// L4 thread blocks waiting for messages, find new one
		backend_reschedule(tag, from, to, timeouts);
	    }
	    else
	    {
		DEBUGGER_ENTER("UNIMPLEMENTED MONITOR L4THREAD EXTERNAL BLOCKING");
	    }
	}
	else
	{

	    L4_Word_t ip;
	    L4_StoreMR( OFS_MR_SAVE_EIP, &ip );
	    PANIC( "Unhandled preemption by tid %t\n", from);
	}
    }
    break;
    case msg_label_preemption_continue:
    {	
	ASSERT(from == vcpu.get_hwirq_tid());
	
	// Preempter
	L4_ThreadId_t preemptee;
	L4_StoreMR(1, &preemptee.raw);
	
	check_pending_virqs(intlogic);
	dprintf(debug_preemption, "got continue IPC preemptee %t \n", preemptee);
	
	if (preemptee == vcpu.main_gtid)
	{
	    // Preempted main
	    L4_StoreMR(2, &tag.raw);
	    
	    L4_Set_MsgTag(tag);
	    vcpu.main_info.mrs.store();
	    backend_reschedule(tag, from, to, timeouts);
	}
	else if (vcpu.user_info && preemptee == vcpu.user_info->get_tid())
	{    
	    // Forward to main
	    tag.X.flags &= ~0x1;
	    L4_Set_MsgTag(tag);
	    to = vcpu.main_gtid;
	}
	else if (vcpu.is_vcpu_thread(preemptee, l4thread_idx))
	{
	    // Preempted l4 thread
	    L4_StoreMR(2, &tag.raw);
	    
	    L4_Set_MsgTag(tag);
	    vcpu.l4thread_info[l4thread_idx].mrs.store();
	    backend_reschedule(tag, from, to, timeouts);
	}
	else 
	{
	    backend_reschedule(tag, from, to, timeouts);
	}

    }
    break;
    default:
    {
	printf("unhandled preemption msg from %t tag %x\n", from, tag.raw);
	DEBUGGER_ENTER("UNIMPLEMENTED");
    }
    break;
    
    }
}


NORETURN void backend_activate_user( iret_handler_frame_t *iret_emul_frame )
{
    vcpu_t &vcpu = get_vcpu();
    L4_MapItem_t map_item;
    L4_ThreadId_t reply = L4_nilthread;
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

    reply = vcpu.user_info->get_tid();
    
    dprintf(debug_iret, "Request to enter user IP %x SP %x FLAGS %x TID %t\n", 
	    iret_emul_frame->iret.ip, iret_emul_frame->iret.sp, 
	    iret_emul_frame->iret.flags.x.raw, vcpu.user_info->get_tid());
    
    if (iret_emul_frame->iret.ip == 0)
	DEBUGGER_ENTER("IRET BUG");
    
    switch (vcpu.user_info->state)
    {
    case thread_state_startup:
    {
	// Prepare the startup IPC
	vcpu.user_info->mrs.load_startup_reply(iret_emul_frame);
	dprintf(debug_task, "> startup %t", reply);
    }
    break;
    case thread_state_exception:
    {
	// Prepare the reply to the exception
	vcpu.user_info->mrs.load_exception_reply(false, iret_emul_frame);
	dump_linux_syscall(vcpu.user_info, false);
    }
    break;
    case thread_state_pfault:
    {
	/* 
	 * jsXXX: maybe we can coalesce both cases (exception and pfault)
	 * and just load the regs accordingly
	 */
	ASSERT( L4_Label(vcpu.user_info->mrs.get_msg_tag()) >= msg_label_pfault_start);
	ASSERT( L4_Label(vcpu.user_info->mrs.get_msg_tag()) <= msg_label_pfault_end);
	backend_handle_user_pagefault(vcpu.user_info, reply, map_item );
	vcpu.user_info->mrs.load_pfault_reply(map_item, iret_emul_frame);
	dprintf(debug_pfault, "> pfault from %t", reply);

    }
    break;
    case thread_state_preemption:
    {
	// Prepare the reply to the exception
	vcpu.user_info->mrs.load_preemption_reply(true, iret_emul_frame);
	dprintf(debug_preemption, "> preemption from %t", reply);
    }
    break;
    case thread_state_activated:
    {	
	vcpu.user_info->mrs.load_activation_reply(iret_emul_frame);
	dprintf(debug_preemption, "> preemption during activation from %t", reply);
    }
    break;
    default:
    {
	printf( "VMEXT Bug invalid user level thread state\n");
	DEBUGGER_ENTER("BUG");
    }
    }
    

    L4_ThreadId_t current = vcpu.user_info->get_tid(), from;
    
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
	    backend_handle_user_exception( vcpu.user_info, vector );
	}
	
	vcpu.user_info->mrs.load();
	L4_MsgTag_t t = L4_MsgTag();
	
	vcpu.dispatch_ipc_enter();
	vcpu.cpu.restore_interrupts( true );
	L4_Accept(L4_UntypedWordsAcceptor);
	tag = L4_ReplyWait( reply, &from );
	
	vcpu.cpu.disable_interrupts();
	vcpu.dispatch_ipc_exit();
	    
	    
	if( L4_IpcFailed(tag) ) {
	    DEBUGGER_ENTER("VMEXT Dispatch IPC error");
	    printf( "VMEXT Dispatch IPC error to %t\n", reply);
	    reply = L4_nilthread;
	    continue;
	}
	reply = L4_nilthread;


	switch( L4_Label(tag) )
	{
	case msg_label_pfault_start ... msg_label_pfault_end:
	    ASSERT(from == current);
	    ASSERT( !vcpu.cpu.interrupts_enabled() );
	    vcpu.user_info->state = thread_state_pfault;
	    vcpu.user_info->mrs.store();
	    backend_handle_user_pagefault( vcpu.user_info, from, map_item );
	    vcpu.user_info->mrs.load_pfault_reply(map_item);
	    reply = current;
	    break;
		
	case msg_label_exception:
	    ASSERT(from == current);
	    vcpu.user_info->state = thread_state_exception;
	    vcpu.user_info->mrs.store();
	    backend_handle_user_exception( vcpu.user_info );
	    vcpu.user_info->mrs.load_exception_reply(true, NULL);
	    reply = current;
	    break;
		
	case msg_label_preemption:
	{
	    vcpu.user_info->state = thread_state_preemption;
	    vcpu.user_info->mrs.store();
	    backend_handle_user_preemption( vcpu.user_info );
	    vcpu.user_info->mrs.load_preemption_reply(true);
	    reply = current;
	    break;
	}
	case msg_label_preemption_continue:
	{
	    ASSERT(from == vcpu.monitor_ltid);
	    L4_ThreadId_t preemptee;
	    L4_StoreMR(1, &preemptee.raw);
	    ASSERT(preemptee == vcpu.user_info->get_tid());
	    L4_StoreMR(2, &tag.raw);
	    L4_Set_MsgTag(tag);
	    vcpu.user_info->state = thread_state_preemption;
	    vcpu.user_info->mrs.store();
	    backend_handle_user_preemption( vcpu.user_info );
	    vcpu.user_info->mrs.load_preemption_reply(true);
	    reply = current;
	    break;
	}

	case msg_label_vector: 
	{
	    msg_vector_extract( (L4_Word_t *) &vector, (L4_Word_t *) &irq );
	    backend_handle_user_exception( vcpu.user_info, vector );
	    break;
	}

	case msg_label_virq: 
	{
	    L4_ThreadId_t ack;
	    msg_virq_extract((L4_Word_t *) &irq, &ack);
	    dprintf(irq_dbg_level(irq), "virtual irq: %d from %t ack %t\n", irq, from, ack);
	    get_intlogic().raise_irq( irq );
	    if (intlogic.pending_vector(vector, irq))
		backend_handle_user_exception( vcpu.user_info, vector );
	    break;
	}
	default:
	    printf( "Unexpected message from TID %t tag %x\n", from, tag.raw);
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

