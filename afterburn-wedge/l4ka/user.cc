/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4-common/user.cc
 * Description:   House keeping code for mapping L4 threads to guest threads.
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
 * $Id: user.cc,v 1.8 2005/04/13 14:35:54 joshua Exp $
 *
 ********************************************************************/
#include <bind.h>
#include <l4/kip.h>
#include <l4/thread.h>
#include <l4/schedule.h>
#include <l4/kip.h>

#include INC_ARCH(bitops.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(memory.h)
#include INC_WEDGE(user.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(l4privileged.h)

static const bool debug_user_pfault=0;
static const bool debug_thread_allocate=1;
static const bool debug_thread_exit=1;

thread_manager_t thread_manager;
task_manager_t task_manager;

L4_Word_t task_info_t::utcb_size = 0;
L4_Word_t task_info_t::utcb_base = 0;

#if defined(CONFIG_L4KA_VMEXTENSIONS)
extern word_t afterburner_helper[];
word_t afterburner_helper_addr = (word_t)afterburner_helper;
#endif

task_info_t::task_info_t()
{
    space_tid = L4_nilthread;
#if defined(CONFIG_VSMP)
    for (word_t id=0; id<CONFIG_NR_VCPUS; id++)
	vcpu_thread[id] = NULL;
#endif
    page_dir = 0;

    for( L4_Word_t i = 0; i < sizeof(utcb_mask)/sizeof(utcb_mask[0]); i++ )
	utcb_mask[i] = 0;
}

void task_info_t::init()
{
    if( 0 == task_info_t::utcb_size ) {
	task_info_t::utcb_size = L4_UtcbSize( L4_GetKernelInterface() );
	task_info_t::utcb_base = get_vcpu().get_kernel_vaddr();
    }

    space_tid = L4_nilthread;
#if defined(CONFIG_VSMP)
    for (word_t id=0; id<CONFIG_NR_VCPUS; id++)
	vcpu_thread[id] = NULL;
#endif
#if defined(CONFIG_L4KA_VMEXTENSIONS)
    for (word_t id=0; id<CONFIG_NR_VCPUS; id++)
	helper_tid[id] = L4_nilthread;
    unmap_count = 0;
#endif    
    for( L4_Word_t i = 0; i < sizeof(utcb_mask)/sizeof(utcb_mask[0]); i++ )
	utcb_mask[i] = 0;
}

bool task_info_t::utcb_allocate( L4_Word_t & utcb, L4_Word_t & uidx )
{
    uidx = bit_allocate_atomic( this->utcb_mask, this->max_threads );
    
    if( uidx >= this->max_threads )
	return false;
    
    utcb = uidx*this->utcb_size + this->utcb_base;
    return true;
}

void task_info_t::utcb_release( L4_Word_t uidx )
{
    static const word_t bits_per_word = sizeof(word_t)*8;
    ASSERT( uidx < this->max_threads );
    bit_clear_atomic( uidx % bits_per_word,
	    this->utcb_mask[uidx / bits_per_word] );
}

word_t task_info_t::thread_count()
    // If the space has only one thread, then it should be the very
    // first utcb index (the space thread).
{

    word_t count = 0;
    for( word_t i = 0; i < sizeof(utcb_mask)/sizeof(utcb_mask[0]); i++ )
    {
	word_t mask = utcb_mask[i];
	
#if 1
	/* works for 32-bit numbers only    */
	register unsigned int mod3 
	    = mask - ((mask >> 1) & 033333333333)
	    - ((mask >> 2) & 011111111111);
	count +=  ((mod3 + (mod3 >> 3)) & 030707070707) % 63;
#else
	while (mask)
	{
	    
	    //count++;
	    //mask &= mask-1;
	}
#endif
    }
    return count;
}

task_manager_t::task_manager_t()
{
}

thread_info_t::thread_info_t()
{
    tid = L4_nilthread;
}

thread_manager_t::thread_manager_t()
{
}

task_info_t *
task_manager_t::find_by_page_dir( L4_Word_t page_dir )
{
    L4_Word_t idx_start = hash_page_dir( page_dir );

    L4_Word_t idx = idx_start;
    do {
	if( tasks[idx].page_dir == page_dir )
	    return &tasks[idx];
	idx = (idx + 1) % max_tasks;
    } while( idx != idx_start );

    // TODO: go to an external process for dynamic memory.
    return 0;
}

task_info_t *
task_manager_t::allocate( L4_Word_t page_dir )
{
    L4_Word_t idx_start = hash_page_dir( page_dir );

    L4_Word_t idx = idx_start;
    do {
	if( tasks[idx].page_dir == 0 ) {
	    tasks[idx].page_dir = page_dir;
	    return &tasks[idx];
	}
	idx = (idx + 1) % max_tasks;
    } while( idx != idx_start );

    // TODO: go to an external process for dynamic memory.
    return 0;
}

void 
task_manager_t::deallocate( task_info_t *ti )
{ 
    //ptab_info.clear(ti->page_dir);
    ti->page_dir = 0; 
}

thread_info_t *
thread_manager_t::find_by_tid( L4_ThreadId_t tid )
{
    L4_Word_t start_idx = hash_tid( tid );

    L4_Word_t idx = start_idx;
    do {
	if( threads[idx].tid == tid )
	    return &threads[idx];
	idx = (idx + 1) % max_threads;
    } while( idx != start_idx );

    // TODO: go to an external process for dynamic memory.
    return 0;
}

thread_info_t *
thread_manager_t::allocate( L4_ThreadId_t tid )
{
    L4_Word_t start_idx = hash_tid( tid );

    L4_Word_t idx = start_idx;
    do {
	if( L4_IsNilThread(threads[idx].tid) ) {
	    threads[idx].tid = tid;
	    return &threads[idx];
	}
	idx = (idx + 1) % max_threads;
    } while( idx != start_idx );

    // TODO: go to an external process for dynamic memory.
    return 0;
}

bool handle_user_pagefault( vcpu_t &vcpu, thread_info_t *thread_info, L4_ThreadId_t tid )
// When entering and exiting, interrupts must be disabled
// to protect the message registers from preemption.
{
    word_t map_addr, map_bits, map_rwx;

    ASSERT( !vcpu.cpu.interrupts_enabled() );

    // Extract the fault info.
    L4_Word_t fault_rwx = thread_info->mr_save.get_pfault_rwx();
    L4_Word_t fault_addr = thread_info->mr_save.get_pfault_addr();
    L4_Word_t fault_ip = thread_info->mr_save.get_pfault_ip();

    if( debug_user_pfault )
	con << "User fault from TID " << tid
	    << ", addr " << (void *)fault_addr
	    << ", ip " << (void *)fault_ip
#if defined(CONFIG_L4KA_VMEXTENSIONS)
	    << ", sp " << (void *)thread_info->mr_save.get(OFS_MR_SAVE_ESP)
#endif
	    << ", rwx " << fault_rwx << '\n';

    if (EXPECT_FALSE(fault_addr == afterburner_helper_addr))
    {
	map_addr = fault_addr;
	map_rwx = 5;
	con << "Helper pfault\n";
	goto done;
    }
    
    // Lookup the translation, and handle the fault if necessary.
    bool complete = backend_handle_user_pagefault( 
	thread_info->ti->get_page_dir(), 
	fault_addr, fault_ip, fault_rwx,
	map_addr, map_bits, map_rwx, thread_info );
    if( !complete )
	return false;

    ASSERT( !vcpu.cpu.interrupts_enabled() );
    
 done:
    // Build the reply message to user.
    L4_MapItem_t map_item = L4_MapItem(
	L4_FpageAddRights(L4_FpageLog2(map_addr, map_bits),
		map_rwx), 
	fault_addr );
    
    if( debug_user_pfault )
	con << "Page fault reply to TID " << tid
	    << ", kernel addr " << (void *)map_addr
	    << ", size " << (1 << map_bits)
	    << ", rwx " << map_rwx
	    << ", user addr " << (void *)fault_addr << '\n';

    
    thread_info->mr_save.load_pfault_reply(map_item);
    return true;
}


thread_info_t *allocate_user_thread()
{
    L4_Error_t errcode;
    vcpu_t &vcpu = get_vcpu();
    L4_ThreadId_t controller_tid = vcpu.main_gtid;

    // Allocate a thread ID.
    L4_ThreadId_t tid = get_hthread_manager()->thread_id_allocate();
    if( L4_IsNilThread(tid) )
	PANIC( "Out of thread IDs." );

    // Lookup the address space's management structure.
    L4_Word_t page_dir = vcpu.cpu.cr3.get_pdir_addr();
    task_info_t *task_info = 
	task_manager_t::get_task_manager().find_by_page_dir( page_dir );
    
    if( !task_info )
    {
	// New address space.
	task_info = task_manager_t::get_task_manager().allocate( page_dir );
	if( !task_info )
	    PANIC( "Hit task limit." );
	task_info->init();
    }

    // Choose a UTCB in the address space.
    L4_Word_t utcb, utcb_index;
    if( !task_info->utcb_allocate(utcb, utcb_index) )
	PANIC( "Hit task thread limit." );

    // Configure the TID.
    tid = L4_GlobalId( L4_ThreadNo(tid), task_info_t::encode_gtid_version(utcb_index) );
    if( task_info_t::decode_gtid_version(tid) != utcb_index )
	PANIC( "L4 thread version wrap-around." );
    
    // Allocate a thread info structure.
    thread_info_t *thread_info = 
	thread_manager_t::get_thread_manager().allocate( tid );
    if( !thread_info )
	PANIC( "Hit thread limit." );
    
    if( !task_info->has_space_tid() ) {
	ASSERT( task_info_t::decode_gtid_version(tid) == 0 );
	task_info->set_space_tid( tid );
	ASSERT( task_info->get_space_tid() == tid );
    }

    
    thread_info->init();
    thread_info->ti = task_info;

    // Init the L4 address space if necessary.
    if( task_info->get_space_tid() == tid )
    {
	// Create the L4 thread.
	errcode = ThreadControl( tid, task_info->get_space_tid(), controller_tid, L4_nilthread, utcb );
	if( errcode != L4_ErrOk )
	    PANIC( "Failed to create initial user thread, TID " << tid 
		    << ", L4 error: " << L4_ErrString(errcode) );

	// Create an L4 address space + thread.
	// TODO: don't hardcode the size of a utcb to 512-bytes
	task_info->utcb_fp = L4_Fpage( user_vaddr_end, 512*CONFIG_L4_MAX_THREADS_PER_TASK );
	task_info->kip_fp = L4_Fpage( L4_Address(task_info->utcb_fp) + L4_Size(task_info->utcb_fp), KB(16) );

	errcode = SpaceControl( tid, 0, task_info->kip_fp, task_info->utcb_fp, L4_nilthread );
	if( errcode != L4_ErrOk )
	    PANIC( "Failed to create an address space, TID " << tid 
		    << ", L4 error: " << L4_ErrString(errcode) );
	
    }

    // Create the L4 thread.
    errcode = ThreadControl( tid, task_info->get_space_tid(), 
	    controller_tid, controller_tid, utcb );
    if( errcode != L4_ErrOk )
	PANIC( "Failed to create user thread, TID " << tid 
		<< ", space TID " << task_info->get_space_tid()
		<< ", utcb " << (void *)utcb 
		<< ", L4 error: " << L4_ErrString(errcode) );
    
    L4_Word_t dummy;
    
#if defined(CONFIG_L4KA_VMEXTENSIONS)
    // Set the thread's exception handler via exregs
    L4_Msg_t msg;
    L4_ThreadId_t local_tid, dummy_tid;
    L4_MsgClear( &msg );
    L4_MsgAppendWord (&msg, controller_tid.raw);
    L4_MsgLoad( &msg );
    local_tid = L4_ExchangeRegisters( tid, (1 << 9), 
	    0, 0, 0, 0, L4_nilthread, 
	    &dummy, &dummy, &dummy, &dummy, &dummy,
	    &dummy_tid );
    if( L4_IsNilThread(local_tid) ) {
	PANIC("Failed to set user thread's exception handler\n");
    }
    
    L4_Word_t preemption_control = L4_PREEMPTION_CONTROL_MSG;
    L4_Word_t time_control = (L4_Never.raw << 16) | L4_Never.raw;
#else
    L4_Word_t preemption_control = ~0UL;
    L4_Word_t time_control = ~0UL;
#endif    
    // Set the thread priority.
    L4_Word_t prio = vcpu.get_vcpu_max_prio() + CONFIG_PRIO_DELTA_USER;    
    
    if (!L4_Schedule(tid, time_control, ~0UL, prio, preemption_control, &dummy))
	PANIC( "Failed to either enable preemption msgs"
		<<" or to set user thread's priority to " << prio 
		<<" or to set user thread's timeslice/quantum to " << (void *) time_control );


    if (debug_thread_allocate)
	con << "create user thread, TID " << tid 
	    << ", space TID " << task_info->get_space_tid()
	    << ", utcb " << (void *)utcb 
	    << "\n";
    
    thread_info->vcpu_id = vcpu.cpu_id;
    // Assign the thread info to the guest OS's thread.
    afterburn_thread_assign_handle( thread_info );

    return thread_info;
}

void delete_user_thread( thread_info_t *thread_info )
{
    if( !thread_info )
	return;
    ASSERT( thread_info->ti );

    L4_ThreadId_t tid = thread_info->get_tid();

    if( thread_info->is_space_thread() ) {
	// Keep the space thread alive, until the address space is empty.
	// Just flip a flag to say that the space thread is invalid.
	if( debug_thread_exit )
	    con << "Space thread invalidate, TID " << tid << '\n';
	thread_info->ti->invalidate_space_tid();
    }
    else
    {
	// Delete the L4 thread.
	thread_info->ti->utcb_release( task_info_t::decode_gtid_version(tid) );
	if( debug_thread_exit )
	    con << "Thread delete, TID " << tid << '\n';
	ThreadControl( tid, L4_nilthread, L4_nilthread, L4_nilthread, ~0UL );
	get_hthread_manager()->thread_id_release( tid );
    }

    if( thread_info->ti->thread_count() == CONFIG_NR_VCPUS
	    && !thread_info->ti->is_space_tid_valid() )
    {
	// Retire the L4 address space.
	tid = thread_info->ti->get_space_tid();
	if( debug_thread_exit )
	    con << "Space delete, TID " << tid << '\n';
	
	ThreadControl( tid, L4_nilthread, L4_nilthread, L4_nilthread, ~0UL );
	get_hthread_manager()->thread_id_release( tid );

	// Release the task info structure.
	task_manager_t::get_task_manager().deallocate( thread_info->ti );
    }

    // Release the thread info structure.
    thread_manager_t::get_thread_manager().deallocate( thread_info );
}


#if defined(CONFIG_L4KA_VMEXTENSIONS)

__asm__ ("						\n\
	.section .text.user, \"ax\"			\n\
	.balign	4096					\n\
afterburner_helper:					\n\
        1:						\n\
	movl	%ebx, -16(%edi)				\n\
	movl	%ebp, -20(%edi)				\n\
	movl	%esi, 4(%edi)				\n\
	movl	%edx, 8(%edi)				\n\
	leal    -48(%edi), %esp				\n\
	movl	-20(%edi), %ebx				\n\
	call	*%ebx	   				\n\
	xorl    %esi, %esi				\n\
	movl	-28(%edi), %ecx				\n\
	movl    %ecx, %ebx				\n\
	andl    $0x3f, %ebx				\n\
	movl	(%edi,%ebx,4), %eax			\n\
	cmpl	-48(%edi), %eax				\n\
	je	3f					\n\
	xchgl	%ecx, %esi				\n\
	2:						\n\
	incl	%ebx					\n\
	movl	(%edi,%ebx,4), %ebp			\n\
	movl	%ebp, 4(%edi,%ecx,4) 			\n\
	incl	%ecx					\n\
	jl	2b					\n\
	movw    $0x12C0, %si				\n\
	3:						\n\
	movl	$-1, %edx				\n\
	xorl    %ecx, %ecx				\n\
	movl	$0x10,-64(%edi)				\n\
	movl    $1b, -52(%edi)		  		\n\
	movl	-16(%edi), %ebx				\n\
	leal    -52(%edi), %esp				\n\
	jmpl    *%ebx					\n\
	.previous					\n\
");


L4_Word_t task_info_t::commit_helper(bool piggybacked=false)
{
    if (unmap_count == 0)
	return 0;

    vcpu_t vcpu = get_vcpu();
    thread_info_t *vcpu_info = vcpu_thread[vcpu.cpu_id]; 
    ASSERT(vcpu_info != NULL);	    
 
    L4_KernelInterfacePage_t *kip = (L4_KernelInterfacePage_t *)
        L4_GetKernelInterface();


   
    if (debug_helper)
	con << "helper "
	    << (piggybacked ? " piggybacked " : "not piggybacked")
	    << " unmap " << unmap_count
	    << "\n";

    L4_MsgTag_t tag = L4_Niltag;
    if (piggybacked) 
    {
	L4_CtrlXferItem_t old_ctrlxfer;
	for (word_t i=0; i<CTRLXFER_SIZE; i++)
	    old_ctrlxfer.raw[3+i] = vcpu_info->mr_save.get(3+i);
	
	L4_Word_t untyped = unmap_count < 2 ? 0 : unmap_count - 2;
	/* We'll piggyback the unmap request to a normal iret */
 	L4_LoadMRs( 1, untyped, (L4_Word_t *) &unmap_pages[2]);	
	L4_LoadMRs( 1 + untyped, CTRLXFER_SIZE, &old_ctrlxfer.raw[3]);
	untyped += CTRLXFER_SIZE;
	
	L4_Word_t utcb_index = decode_gtid_version(vcpu_info->get_tid());
	L4_Word_t utcb = utcb_index*utcb_size + utcb_base + 0x100;
	vcpu_info->mr_save.set(OFS_MR_SAVE_EIP, (word_t) afterburner_helper);
	vcpu_info->mr_save.set(OFS_MR_SAVE_EDI, utcb);
	vcpu_info->mr_save.set(OFS_MR_SAVE_ESI, unmap_pages[0].raw);
	vcpu_info->mr_save.set(OFS_MR_SAVE_EDX, unmap_pages[1].raw);
	vcpu_info->mr_save.set(OFS_MR_SAVE_EAX, unmap_count-1);
	vcpu_info->mr_save.set(OFS_MR_SAVE_EBX, L4_Address(kip_fp)+ kip->Ipc);	
	vcpu_info->mr_save.set(OFS_MR_SAVE_EBP, L4_Address(kip_fp)+ kip->Unmap);	
	L4_SetCtrlXferMask(&old_ctrlxfer, 0x3ff);
	unmap_count = 0;
	return untyped;

    }
    
    ASSERT(false);
    DEBUGGER_ENTER(0);
    tag.X.u = unmap_count + 1;
	
    /*
     *  We go into dispatch mode; if the helper should be preempted,
     *  we'll get scheduled by the IRQ thread.
     */
    
 
    for (;;)
    {	
	L4_Set_MsgTag(tag);
	vcpu.dispatch_ipc_enter();
	tag = L4_Call(helper_tid[vcpu.cpu_id]);
	vcpu.dispatch_ipc_exit();
	
	switch (L4_Label(tag))
	{
	    case 0:
	    {
		/* Helper waits */
		return 0;
	    }
	    break;
	    case msg_label_preemption:
	    {
		if (debug_helper)
		    con << "helper restart \n";
		tag = L4_Niltag;
		break;
	    }
	    default:
	    {
		con << "VMEXT Bug invalid helper thread state\n";
		DEBUGGER_ENTER();
		
	    }
	    break;
	}
    }
}

#endif /* defined(CONFIG_L4KA_VMEXTENSIONS) */

		 
	

    
