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
static const bool debug_helper=1;

thread_manager_t thread_manager;
task_manager_t task_manager;

L4_Word_t task_info_t::utcb_size = 0;
L4_Word_t task_info_t::utcb_base = 0;

task_info_t::task_info_t()
{
    space_tid = L4_nilthread;
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
#if defined(CONFIG_L4KA_VMEXTENSIONS)
    helper_tid = L4_nilthread;
    unmap_count = 0;
#endif    
    for( L4_Word_t i = 0; i < sizeof(utcb_mask)/sizeof(utcb_mask[0]); i++ )
	utcb_mask[i] = 0;
}

bool task_info_t::utcb_allocate( L4_Word_t & utcb, L4_Word_t & uidx )
{
    uidx = bit_allocate_atomic( this->utcb_mask, this->max_threads );
    
#if defined(CONFIG_L4KA_VMEXTENSIONS)
    const L4_Word_t max = this->max_threads-1;
#else
    const L4_Word_t max = this->max_threads;
#endif
    if( uidx >= max )
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

bool task_info_t::has_one_thread()
    // If the space has only one thread, then it should be the very
    // first utcb index (the space thread).
{
    bool has_one = utcb_mask[0] == 1;
    if( !has_one )
	return false;

    for( word_t i = 1; i < sizeof(utcb_mask)/sizeof(utcb_mask[0]); i++ )
	if( utcb_mask[i] )
	    return false;

    return true;
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

    // Lookup the translation, and handle the fault if necessary.
    bool complete = backend_handle_user_pagefault( 
	    thread_info->ti->get_page_dir(), 
	    fault_addr, fault_ip, fault_rwx,
	    map_addr, map_bits, map_rwx, thread_info );
    if( !complete )
	return false;

    ASSERT( !vcpu.cpu.interrupts_enabled() );

    // Build the reply message to user.
    if( debug_user_pfault )
	con << "Page fault reply to TID " << tid
	    << ", kernel addr " << (void *)map_addr
	    << ", size " << (1 << map_bits)
	    << ", rwx " << map_rwx
	    << ", user addr " << (void *)fault_addr << '\n';
    
    L4_MapItem_t map_item = L4_MapItem(
	    L4_FpageAddRights(L4_FpageLog2(map_addr, map_bits),
		map_rwx), 
	    fault_addr );
    
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
    if( !task_info->has_space_tid() ) {
	ASSERT( task_info_t::decode_gtid_version(tid) == 0 );
	task_info->set_space_tid( tid );
	ASSERT( task_info->get_space_tid() == tid );
    }

    // Allocate a thread info structure.
    thread_info_t *thread_info = 
	thread_manager_t::get_thread_manager().allocate( tid );
    if( !thread_info )
	PANIC( "Hit thread limit." );
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
	
#if defined(CONFIG_L4KA_VMEXTENSIONS)
	task_info->allocate_helper();
#endif

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

    if( thread_info->ti->has_one_thread() 
	    && !thread_info->ti->is_space_tid_valid() )
    {
	// Retire the L4 address space.
	tid = thread_info->ti->get_space_tid();
	if( debug_thread_exit )
	    con << "Space delete, TID " << tid << '\n';
	
#if defined(CONFIG_L4KA_VMEXTENSIONS)
	thread_info->ti->release_helper();
#endif
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
	movl	%gs:0, %edi				\n\
	movl	-48(%edi), %edx				\n\
	movl	%edx, -44(%edi)				\n\
	movl	%ebx, -16(%edi)				\n\
	movl	%ebp, -20(%edi)				\n\
	movl	-48(%edi), %edx				\n\
	xorl    %eax, %eax				\n\
	xorl    %esi, %esi				\n\
	jmp	2f					\n\
	1:						\n\
	movl    %esi, %eax				\n\
	andl    $0x3f, %eax				\n\
	decl    %eax	       			        \n\
	movl	%esi, -28(%edi)				\n\
	xchgl	(%edi,%eax,4), %esi			\n\
	decl    %eax	       			        \n\
	orl     $0x40, %eax				\n\
	movl	-20(%edi), %ebx				\n\
	leal    -48(%edi), %esp				\n\
	call	*%ebx	   				\n\
	movl	-28(%edi), %esi				\n\
	movl    %esi, %ebx				\n\
	andl    $0x3f, %ebx				\n\
	movl	(%edi,%ebx,4), %eax			\n\
	movl	-48(%edi), %edx				\n\
	movl	%edx, -24(%edi)				\n\
	orl     $0x1000, %esi				\n\
	2:						\n\
	xorl    %ecx, %ecx				\n\
	movl	$0x10,-64(%edi)				\n\
	movl    $1b, -52(%edi)		  		\n\
	movl	-16(%edi), %ebx				\n\
	leal    -52(%edi), %esp				\n\
	jmpl    *%ebx					\n\
	.previous					\n\
");

extern word_t afterburner_helper[];
word_t afterburner_helper_addr = (word_t)afterburner_helper;

void task_info_t::allocate_helper()
{
    if (!L4_IsNilThread(helper_tid))
	return;

    vcpu_t &vcpu = get_vcpu();
    L4_Error_t errcode;
    L4_MsgTag_t tag;
    L4_CtrlXferItem_t ctrlxfer;

    // Allocate a thread ID.
    helper_tid = get_hthread_manager()->thread_id_allocate();
    if( L4_IsNilThread(helper_tid) )
	PANIC( "Out of thread IDs for helper_tid." );

    L4_Word_t utcb_index = max_threads - 1;
    L4_Word_t utcb = utcb_index*utcb_size + utcb_base;
    
    // Configure the TID.
    helper_tid = L4_GlobalId( L4_ThreadNo(helper_tid), encode_gtid_version(utcb_index) );
    if( decode_gtid_version(helper_tid) != utcb_index )
	PANIC( "L4 thread version wrap-around for helper_tid." );
		    
    // Create the L4 thread.
    errcode = ThreadControl( helper_tid, get_space_tid(), 
	    L4_Myself(), L4_Myself(), utcb );
    if( errcode != L4_ErrOk )
	PANIC( "Failed to create unmap thread, TID " << helper_tid 
		<< ", space TID " << get_space_tid()
		<< ", utcb " << (void *)utcb 
		<< ", L4 error: " << L4_ErrString(errcode) );
	    
    if (debug_helper)
	con << "Allocating helper tid " << helper_tid << "\n";

    L4_Word_t preemption_control = L4_PREEMPTION_CONTROL_MSG;
    L4_Word_t time_control = (L4_Never.raw << 16) | L4_Never.raw;
    L4_Word_t prio = vcpu.get_vcpu_max_prio() + CONFIG_PRIO_DELTA_HELPER;    
    L4_Word_t dummy;
    if (!L4_Schedule(helper_tid, time_control, ~0UL, prio, preemption_control, &dummy))
	PANIC("Error: unable to either enable preemption msgs"
		<< " or to set unmap thread's priority to " << prio 
		<< " or to set unmap thread's timeslice/quantum to " << (void *) time_control
		<< "\n");
	
    /* Let helper thread wait for unmap ipc */
    L4_KernelInterfacePage_t *kip = (L4_KernelInterfacePage_t *) 
	L4_GetKernelInterface();

    L4_Msg_t msg;
    L4_MsgClear( &msg );
    L4_InitCtrlXferItem(&ctrlxfer);
    L4_SetCtrlXferMask(&ctrlxfer, 0x3ff);
    ctrlxfer.eip = afterburner_helper_addr;
    ctrlxfer.ebx = L4_Address(kip_fp) + kip->Ipc;
    ctrlxfer.ebp = L4_Address(kip_fp) + kip->Unmap;
    L4_AppendCtrlXferItem(&msg, &ctrlxfer);
    L4_MsgLoad( &msg );
	
    L4_Call(helper_tid);
	
    L4_StoreMR( 0, &tag.raw);
    ASSERT( L4_Label(tag) >= msg_label_pfault_start && L4_Label(tag) <= msg_label_pfault_end); 

    L4_MapItem_t map_item = L4_MapItem(
	L4_FpageAddRights(
	    L4_FpageLog2((L4_Word_t) afterburner_helper_addr, PAGE_BITS), 0x5 ),
	afterburner_helper_addr );
	
    L4_MsgClear( &msg );
    L4_MsgAppendMapItem( &msg, map_item );
    L4_MsgLoad( &msg );
	
    L4_Reply(helper_tid);
    
}

void task_info_t::release_helper()
{
    if (!L4_IsNilThread(helper_tid))
    {
	if (debug_helper || 1)
	    con << "Deallocating helper tid " << helper_tid << "\n";
	ThreadControl( helper_tid, L4_nilthread, L4_nilthread, L4_nilthread, ~0UL );
	helper_tid = L4_nilthread;
    }
}


L4_Word_t task_info_t::commit_helper(L4_ThreadId_t &reply_tid)
{

    if (unmap_count == 0)
	return 0;
    
    ASSERT(helper_tid != L4_nilthread);	    
    
    vcpu_t &vcpu = get_vcpu();
    
    if (debug_helper)
	con << "helper "
	    << " reply " << reply_tid
	    << " unmap " << unmap_count
	    << "\n";

    L4_MsgTag_t tag = L4_Niltag;
    if (reply_tid == L4_nilthread) 
	reply_tid = L4_Myself();
    
    tag.X.u = unmap_count + 1;

    L4_LoadMRs( 1, unmap_count, (L4_Word_t *) &unmap_pages);
    L4_LoadMRs( 1 + unmap_count, 1, &reply_tid.raw);
    
    /*
     *  We go into dispatch mode; if the helper should be preempted,
     *  we'll get scheduled by the IRQ thread.
     */
    
    if (reply_tid != L4_Myself())
    {
	reply_tid = helper_tid;
	return tag.X.u;
    }
  
restart:  
    L4_LoadMR (  0, tag.raw);
    vcpu.dispatch_ipc_enter();
    L4_Call(helper_tid);
    vcpu.dispatch_ipc_exit();
    L4_StoreMR( 0, &tag.raw);
	
    if (L4_Label(tag)  == msg_label_preemption)
    {
	if (debug_helper)
	    con << "helper restart done \n";
	tag = L4_Niltag;
	goto restart;
    }
  
    if (debug_helper)
	con << "helper done \n";
    
    
    unmap_count = 0;
    
    return 0;
}

#endif /* defined(CONFIG_L4KA_VMEXTENSIONS) */

		 
	

    
