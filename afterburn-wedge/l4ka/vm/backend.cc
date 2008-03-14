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
#include <bind.h>
#include <l4/schedule.h>
#include <l4/ipc.h>

#include INC_WEDGE(vm.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(irq.h)



typedef void (*vm_entry_t)();
word_t user_vaddr_end = 0x80000000;

__asm__ ("					\n\
	.section .text.user, \"ax\"		\n\
	.balign	4096				\n\
afterburner_user_startup:			\n\
	movl	%gs:0, %eax			\n\
	movl	-48(%eax), %ebx			\n\
	movl	%ebx, -44(%eax)			\n\
afterburner_user_force_except:			\n\
	int	$0x0				\n\
	.previous				\n\
");

extern word_t afterburner_user_startup[];
word_t afterburner_user_start_addr = (word_t)afterburner_user_startup;

INLINE bool async_safe( word_t ip )
{
    return ip < CONFIG_WEDGE_VIRT;
}


bool deliver_ia32_exception( cpu_t & cpu, L4_Word_t vector, u32_t error_code, thread_info_t *thread_info)
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
    
    L4_Word_t dummy;
    L4_ThreadId_t dummy_tid, res;
    L4_ThreadId_t main_ltid = get_vcpu().main_ltid;

    // Read registers of page fault.
    // TODO: get rid of this call to exchgregs ... perhaps enhance page fault
    // protocol with more info.
    res = L4_ExchangeRegisters( main_ltid, 0, 0, 0, 0, 0, L4_nilthread,
	    (L4_Word_t *)&dummy, 
	    (L4_Word_t *)&old_esp, 
	    (L4_Word_t *)&old_eip, 
	    (L4_Word_t *)&old_eflags, 
	    (L4_Word_t *)&dummy, &dummy_tid );
    if( L4_IsNilThread(res) )
	return false;

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

    // Update thread registers with target execution point.
    res = L4_ExchangeRegisters( main_ltid, 3 << 3 /* i,s */,
				(L4_Word_t) esp, gate.get_offset(), 0, 0, L4_nilthread,
				&dummy, &dummy, &dummy, &dummy, &dummy, &dummy_tid );
    if( L4_IsNilThread(res) )
	return false;
    
    return true;
}

void NORETURN deliver_ia32_user_exception( cpu_t &cpu, L4_Word_t vector, 
	bool use_error_code, L4_Word_t error_code, L4_Word_t ip )
{
    
    ASSERT( vector <= cpu.idtr.get_total_gates() );
    gate_t *idt = cpu.idtr.get_gate_table();
    gate_t &gate = idt[ vector ];

    ASSERT( gate.is_trap() || gate.is_interrupt() );
    ASSERT( gate.is_present() );
    ASSERT( gate.is_32bit() );

    flags_t old_flags = cpu.flags;
    old_flags.x.fields.fi = 1;  // Interrupts are usually enabled at user.
    cpu.flags.prepare_for_gate( gate );

    tss_t *tss = cpu.get_tss();
    dprintf(debug_pfault, "tss esp0 %x ss0 %x\n", tss->esp0, tss->ss0);

    u16_t old_cs = cpu.cs;
    u16_t old_ss = cpu.ss;
    cpu.cs = gate.get_segment_selector();
    cpu.ss = tss->ss0;

    if( gate.is_trap() )
	cpu.restore_interrupts( true );

    word_t *stack = (word_t *)tss->esp0;
    *(--stack) = old_ss;	// User ss
    *(--stack) = 0;		// User sp
    *(--stack) = old_flags.get_raw();	// User flags
    *(--stack) = old_cs;	// User cs
    *(--stack) = ip;		// User ip
    if( use_error_code )
	*(--stack) = error_code;

    __asm__ __volatile__ (
	    "movl	%0, %%esp ;"	// Switch stack
	    "jmp	*%1 ;"		// Activate gate
	    : : "r"(stack), "r"(gate.get_offset())
	    );

    panic();
}

void NORETURN
backend_handle_user_exception( thread_info_t *thread_info, word_t vector )
{
    deliver_ia32_user_exception( get_cpu(), vector, false, 0, 0 ); 
}


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

extern "C" void async_irq_handle_exregs( void );
__asm__ ("\n\
	.text								\n\
	.global async_irq_handle_exregs					\n\
async_irq_handle_exregs:						\n\
	pushl	%eax							\n\
	pushl	%ebx							\n\
	movl	(4+8)(%esp), %eax	/* old stack */			\n\
	subl	$16, %eax		/* allocate iret frame */	\n\
	movl	(16+8)(%esp), %ebx	/* old flags */			\n\
	movl	%ebx, 12(%eax)						\n\
	movl	(12+8)(%esp), %ebx	/* old cs */			\n\
	movl	%ebx, 8(%eax)						\n\
	movl	(8+8)(%esp), %ebx	/* old eip */			\n\
	movl	%ebx, 4(%eax)						\n\
	movl	(0+8)(%esp), %ebx	/* new eip */			\n\
	movl	%ebx, 0(%eax)						\n\
	xchg	%eax, %esp		/* swap to old stack */		\n\
	movl	(%eax), %ebx		/* restore ebx */		\n\
	movl	4(%eax), %eax		/* restore eax */		\n\
	ret				/* activate handler */		\n\
	");


/*
 * Returns if redirection was necessary
 */
bool backend_async_deliver_irq( intlogic_t &intlogic )
{
    vcpu_t &vcpu = get_vcpu();
    cpu_t &cpu = vcpu.cpu;

    ASSERT( L4_MyLocalId() != vcpu.main_ltid );
    
    word_t vector, irq;

    if( !cpu.interrupts_enabled() )
	return false;
    if( !intlogic.pending_vector(vector, irq) )
	return false;

   
    ASSERT( vector < cpu.idtr.get_total_gates() );
    gate_t *idt = cpu.idtr.get_gate_table();
    gate_t &gate = idt[ vector ];

    ASSERT( gate.is_trap() || gate.is_interrupt() );
    ASSERT( gate.is_present() );
    ASSERT( gate.is_32bit() );

    dprintf(irq_dbg_level(irq), "interrupt deliver vector %d handler %x\n", vector, gate.get_offset());
 
    L4_Word_t old_esp, old_eip, old_eflags;
    
    static const L4_Word_t temp_stack_words = 64;
    static L4_Word_t temp_stacks[ temp_stack_words * CONFIG_NR_VCPUS ];
    L4_Word_t dummy;
    
    L4_Word_t *esp = (L4_Word_t *)
	&temp_stacks[ (vcpu.get_id()+1) * temp_stack_words ];

    esp = &esp[-5]; // Allocate room on the temp stack.

    L4_ThreadId_t dummy_tid, result_tid;
    L4_ThreadState_t l4_tstate;
    
    result_tid = L4_ExchangeRegisters( vcpu.main_ltid, (3 << 3) | 2,
	    (L4_Word_t)esp, (L4_Word_t)async_irq_handle_exregs,
	    0, 0, L4_nilthread,
	    &l4_tstate.raw, &old_esp, &old_eip, &old_eflags,
	    &dummy, &dummy_tid );
    
    ASSERT( !L4_IsNilThread(result_tid) );

    if( EXPECT_FALSE(!async_safe(old_eip)) ) 
	// We are already executing somewhere in the wedge.
    {
	// Cancel the interrupt delivery.
	INC_BURN_COUNTER(async_delivery_canceled);
	intlogic.reraise_vector( vector, irq ); // Reraise the IRQ.
	
	// Resume execution at the original esp + eip.
	result_tid = L4_ExchangeRegisters( vcpu.main_ltid, (3 << 3) | 2,
		old_esp, old_eip, 0, 0, 
		L4_nilthread, &l4_tstate.raw, 
		&old_esp, &old_eip, &old_eflags, 
		&dummy, &dummy_tid );
	ASSERT( !L4_IsNilThread(result_tid) );
	ASSERT( old_eip == (L4_Word_t)async_irq_handle_exregs );
	return false;
    }
    
    cpu.flags.x.raw = (old_eflags & flags_user_mask) | (cpu.flags.x.raw & ~flags_user_mask);
    
    // Store values on the stack.
    esp[4] = cpu.flags.x.raw;
    esp[3] = cpu.cs;
    esp[2] = old_eip;
    esp[1] = old_esp;
    esp[0] = gate.get_offset();
  
    // Side effects are now permitted to the CPU object.
    cpu.flags.prepare_for_gate( gate );
    cpu.cs = gate.get_segment_selector();
    return true;
}

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

    dprintf(debug_idle, "Entering idle\n");

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
    
#if 0
    if (!L4_IsLocalId(tid))
    {
	printf( "Unexpected IPC in idle loop, from non-local TID %t tag %x\n", tid, tag.raw);
	return;
    }
#endif
    if( L4_IpcSucceeded(tag) ) 
	switch (L4_Label(tag))
	{
	case msg_label_vector:
	{
	    L4_Word_t vector, irq;
	    msg_vector_extract( &vector, &irq);
	    ASSERT( !redirect_frame->is_redirect() );
	    redirect_frame->do_redirect( vector );
	    dprintf(irq_dbg_level(0, vector), " idle VM %t received vector %d\n", L4_Myself(), vector);
	    break;
	}	    
	case msg_label_virq:
	{
	    L4_Word_t irq;
	    msg_virq_extract( &irq );
	    
	    get_intlogic().raise_irq(irq);
	    ASSERT( !redirect_frame->is_redirect() );
	    redirect_frame->do_redirect();
	    break;
	}	    
	default:
	    printf( "Unexpected IPC in idle loop from %t tag %x\n", tid, tag.raw);
	    break;
	}
    else {
	printf( "IPC failure in idle loop.  L4 error code %d\n", err);
    }
}    


static void delay_message( L4_MsgTag_t tag, L4_ThreadId_t from_tid )
{
    // Message isn't from the "current" thread.  Delay message 
    // delivery until Linux chooses to schedule the from thread.
    thread_info_t *thread_info = 
	get_thread_manager().find_by_tid( from_tid );
    if( !thread_info ) {
	printf( "Unexpected message from TID %t\n", from_tid);
	DEBUGGER_ENTER("unexpected msg");
	return;
    }

    thread_info->mr_save.store(tag);
    thread_info->state = thread_state_pending;
}

static void handle_forced_user_pagefault( vcpu_t &vcpu, L4_Word_t fault_rwx, L4_Word_t fault_addr, 
					  L4_Word_t fault_ip, L4_MsgTag_t tag, L4_ThreadId_t from_tid )
{
    extern word_t afterburner_user_start_addr;

    ASSERT( !vcpu.cpu.interrupts_enabled() );

    ASSERT( fault_addr >= user_vaddr_end );
    ASSERT( fault_rwx & 0x5 );

    L4_MapItem_t map_item = L4_MapItem(
	L4_FpageAddRights(
	    L4_FpageLog2(afterburner_user_start_addr, PAGE_BITS), 0x5 ),
	fault_addr );
    L4_Msg_t msg;
    L4_MsgClear( &msg );
    L4_MsgAppendMapItem( &msg, map_item );
    L4_MsgLoad( &msg );
}




NORETURN void backend_activate_user( iret_handler_frame_t *iret_emul_frame )
{
    vcpu_t &vcpu = get_vcpu();
    bool complete;

    dprintf(debug_iret, "Request to enter user IP %x SP %x\n", 
	    iret_emul_frame->iret.ip, iret_emul_frame->iret.sp);

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
	    delete_thread( thread_info );
	}

	// We are starting a new thread, so the reply message is the
	// thread startup message.
	thread_info = allocate_thread();
	afterburn_thread_assign_handle( thread_info );
	reply_tid = thread_info->get_tid();
	dprintf(debug_task, "New thread start, TID %t\n",thread_info->get_tid());
	thread_info->state = thread_state_force;
	// Prepare the reply to the forced exception
	thread_info->mr_save.load_startup_reply(user_vaddr_end + 0x1000000, 0, iret_emul_frame);
    }
    else if( thread_info->state == thread_state_except_reply )
    {
	reply_tid = thread_info->get_tid();
	thread_info->state = thread_state_user;
	dump_linux_syscall(thread_info, false);
	// Prepare the reply to the exception
	thread_info->mr_save.load_exception_reply(false, iret_emul_frame);

    }
    else if( thread_info->state == thread_state_pending )
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
	    thread_info->state = thread_state_pending;
	    L4_MapItem_t map_item;
	    complete = backend_handle_user_pagefault( thread_info, reply_tid, map_item );
	    ASSERT( complete );
	    thread_info->mr_save.load_pfault_reply(map_item);
	    break;

	case msg_label_exception:
	    thread_info->state = thread_state_except_reply;
	    backend_handle_user_exception( thread_info );
	    panic();
	    break;

	default:
	    reply_tid = L4_nilthread;	// No message to user.
	    break;
	}
	// Clear the pre-existing message to prevent replay.
	thread_info->mr_save.set_msg_tag(L4_Niltag);
	thread_info->state = thread_state_user;
    }
    else
    {
	// No pending message to answer.  Thus the app is already at
	// L4 user, with no expectation of a message from us.
	reply_tid = L4_nilthread;
	if( iret_emul_frame->iret.ip != 0 )
	    printf( "Attempted signal delivery during async interrupt.\n");
    }

    L4_ThreadId_t current_tid = thread_info->get_tid(), from_tid;

    for(;;)
    {
	// Send and wait for message.
	// Disable preemption to avoid race conditions with virtual, 
	// asynchronous interrupts.  TODO: make this work with interrupts of
	// physical devices too (i.e., lower their priorities).
	L4_DisablePreemption();
	vcpu.dispatch_ipc_enter();
	vcpu.cpu.restore_interrupts( true );
	L4_MsgTag_t tag = L4_ReplyWait( reply_tid, &from_tid );
	vcpu.cpu.disable_interrupts();
	vcpu.dispatch_ipc_exit();
	L4_EnablePreemption();
	if( L4_PreemptionPending() )
	    L4_Yield();

	reply_tid = L4_nilthread;

	if( L4_IpcFailed(tag) ) {
	    DEBUGGER_ENTER("Dispatch IPC Error");
	    printf( "Dispatch IPC error.\n");
	    continue;
	}

	switch( L4_Label(tag))
	{
	case msg_label_pfault_start ... msg_label_pfault_end:
	    if( EXPECT_FALSE(from_tid != current_tid) ) {
		dprintf(debug_pfault, "Delayed user page fault from TID %t\n", from_tid);
		delay_message( tag, from_tid );
	    }
	    else if( thread_info->state == thread_state_force ) {
		// We have a pending register set and want to preserve it.
		// The only page fault possible is on L4-specific code.
		L4_Word_t fault_addr, fault_ip, fault_rwx;
		fault_rwx = L4_Label(tag) & 0x7;
		L4_StoreMR( 1, &fault_addr );
		L4_StoreMR( 2, &fault_ip );

		dprintf(debug_task,  "Forced user page fault addr %x ip %x TID %x\n", 
			fault_addr, fault_ip, from_tid);

		handle_forced_user_pagefault( vcpu, fault_rwx, fault_addr, fault_ip, tag, from_tid );
		reply_tid = current_tid;
		// Maintain state_force
	    }
	    else {
		thread_info->state = thread_state_pending;
		thread_info->mr_save.set_msg_tag(tag);
		ASSERT( !vcpu.cpu.interrupts_enabled() );
		thread_info->mr_save.store(tag);
		L4_MapItem_t map_item;
		complete = backend_handle_user_pagefault( thread_info, from_tid, map_item );
		if( complete ) {
		    // Immediate reply.
		    thread_info->mr_save.load_pfault_reply(map_item);
		    reply_tid = current_tid;
		    thread_info->state = thread_state_user;
		}
	    }
	    continue;

	case msg_label_exception:
	    if( EXPECT_FALSE(from_tid != current_tid) )
		delay_message( tag, from_tid );
	    else if( EXPECT_FALSE(thread_info->state == thread_state_force) ) {
		// We forced this exception.  Respond with the pending 
		// register set.
		dprintf(debug_task,  "Official user start TID %x\n", current_tid);
		thread_info->mr_save.set_msg_tag(tag);
		thread_info->mr_save.load();
		reply_tid = current_tid;
		thread_info->state = thread_state_user;
	    }
	    else {
		thread_info->state = thread_state_except_reply;
		thread_info->mr_save.store(tag);
		backend_handle_user_exception( thread_info );
		panic();
	    }
	    continue;
	case msg_label_vector: 
	{
	    L4_Word_t vector, irq;
	    msg_vector_extract( &vector, &irq );
	    backend_handle_user_exception( thread_info, vector );
	    panic();
	    break;
	}

	case msg_label_virq: 
	{
	    L4_Word_t msg_irq;
	    word_t irq, vector;
	    msg_virq_extract( &msg_irq );
	    get_intlogic().raise_irq( msg_irq );
	    if( get_intlogic().pending_vector(vector, irq) )
		backend_handle_user_exception( thread_info, vector );
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


void backend_exit_hook( void *handle )
{
    cpu_t &cpu = get_cpu();
    bool saved_int_state = cpu.disable_interrupts();
    delete_thread( (thread_info_t *)handle );
    cpu.restore_interrupts( saved_int_state );
}

int backend_signal_hook( void *handle )
// Return 1 to cancel signal delivery.
// Return 0 to permit signal delivery.
{
    thread_info_t *thread_info = (thread_info_t *)handle;
    if( EXPECT_FALSE(!thread_info) )
	return 0;

    if( thread_info->state != thread_state_except_reply ) {
	dprintf(debug_user_signal, "Delayed signal delivery.\n");
	return 1;
    }

    return 0;
}

