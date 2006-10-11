/*********************************************************************
 *
 * Copyright (C) 2005, University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4-common/hthread.c
 * Description:   The hthread library; provides simple wrappers for
 *                L4 thread management.
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
 * $Id: hthread.cc,v 1.11 2005/08/26 15:43:47 joshua Exp $
 *
 ********************************************************************/
#include <l4/thread.h>
#include <l4/schedule.h>
#include <l4/kip.h>

#include INC_ARCH(bitops.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(vcpulocal.h)

#include <memory.h>
#include <bind.h>

hthread_manager_t hthread_manager;

static const bool debug_thread_exit=0;

/* hthread_manager_t Implementation.
 */
void hthread_manager_t::init( L4_Word_t tid_space_start, L4_Word_t tid_space_len )
{
    // Zero the masks.
    L4_Word_t i;
    for( i = 0; i < sizeof(utcb_mask)/sizeof(utcb_mask[0]); i++ )
	utcb_mask[i] = 0;
    for( i = 0; i < sizeof(tid_mask)/sizeof(tid_mask[0]); i++ )
	tid_mask[i] = 0;

    this->thread_space_start = tid_space_start;
    this->thread_space_len = tid_space_len;
    if( this->thread_space_len > hthread_manager_t::max_threads )
	this->thread_space_len = hthread_manager_t::max_threads;

    // Update the TID bitmask to reflect our currently running thread.
    L4_Word_t my_tidx = L4_ThreadNo(L4_Myself()) - this->thread_space_start;
    bit_set_atomic( my_tidx % sizeof(word_t), 
	    this->tid_mask[my_tidx / sizeof(word_t)] );

    this->utcb_size = L4_UtcbSize( L4_GetKernelInterface() );
    this->utcb_base = L4_MyLocalId().raw & ~(utcb_size - 1);
    // Adjust to correlate with thread_space_start.
    this->utcb_base -= my_tidx*this->utcb_size;

    // Update the UTCB bitmask to reflect our currently running thread.
    L4_Word_t my_uidx = (L4_MyLocalId().raw - this->utcb_base)/this->utcb_size;
    bit_set_atomic( my_uidx % sizeof(word_t), 
	    this->utcb_mask[my_uidx / sizeof(word_t)] );
}

hthread_t * hthread_manager_t::create_thread( 
	L4_Word_t stack_bottom,
	L4_Word_t stack_size,
	L4_Word_t prio,
	hthread_func_t start_func,
	L4_ThreadId_t scheduler_tid,
	L4_ThreadId_t pager_tid,
	void *start_param,
	void *tlocal_data,
	L4_Word_t tlocal_size)
{
    L4_Error_t errcode;

    if( tlocal_size > stack_size/2 )
    {
	con << "Error: stack size is too small for the thread local data.\n";
	return NULL;
    }

    L4_ThreadId_t tid = this->thread_id_allocate();
    if( L4_IsNilThread(tid) ) {
	con << "Error: out of thread ID's.\n";
	return NULL;
    }

    // Is the thread already running?
    L4_Word_t result;
    L4_ThreadId_t local_tid, dummy_tid;
    local_tid = L4_ExchangeRegisters( tid, 0, 0, 0, 0, 0, L4_nilthread, 
	    &result, &result, &result, &result, &result, &dummy_tid );
    if( !L4_IsNilThread(local_tid) ) {
	con << "Error: attempt to recreate a running thread.\n";
	this->thread_id_release( tid );
	return NULL;
    }

    // Create the thread.
    L4_Word_t utcb = this->utcb_allocate();
    if( !utcb ) {
	con << "Error: out of UTCB space.\n";
	this->thread_id_release( tid );
	return NULL;
    }
    errcode = ThreadControl( tid, L4_Myself(), scheduler_tid, pager_tid, utcb );
    if( errcode != L4_ErrOk ) {
	con << "Error: unable to create a thread, L4 error: " 
	    << L4_ErrString(errcode) << ".\n";
	this->thread_id_release( tid );
	return NULL;
    }

    // Priority
    if( !L4_Set_Priority(tid, prio) )
    {
	con << "Error: unable to set a thread's priority to " << prio
	    << ", L4 error code: " << L4_ErrString(L4_ErrorCode()) << '\n';
	this->thread_id_release( tid );
	return NULL;
    }

    // Create the thread's stack.
    L4_Word_t sp, ip;
    sp = stack_bottom + stack_size;
    sp -= sizeof(hthread_t);
    hthread_t *hthread = (hthread_t *)sp;
    hthread->tlocal_data = NULL;
    hthread->start_func = start_func;
    hthread->start_param = start_param;
    hthread->stack_memory = stack_bottom;
    hthread->stack_size = stack_size;

    if( tlocal_data != NULL )
    {
	// Put the thread local data on the stack.
	sp -= tlocal_size;
	hthread->tlocal_data = (void *)sp;
	memcpy( hthread->tlocal_data, tlocal_data, tlocal_size );
    }

    // Ensure that the stack conforms to the function calling ABI.
    sp = (sp - CONFIG_STACK_SAFETY) & ~(CONFIG_STACK_ALIGN-1);

    // Let architecture-specific code prepare for the thread-start trampoline.
    hthread->arch_prepare_exreg( sp, ip );

    // Set the thread's starting SP and starting IP.
    local_tid = L4_ExchangeRegisters( tid, (3 << 3) | (1 << 6), 
	    sp, ip, 0, L4_Word_t(hthread),
	    L4_nilthread, &result, &result, &result, &result, &result,
	    &dummy_tid );
    if( L4_IsNilThread(local_tid) )
    {
	con << "Error: unable to setup a thread, L4 error code: "
	    << L4_ErrString(L4_ErrorCode()) << '\n';
	this->thread_id_release( tid );
	return NULL;
    }

    hthread->local_tid = local_tid;
    return hthread;
}

L4_ThreadId_t hthread_manager_t::thread_id_allocate()
{
    L4_Word_t tidx = bit_allocate_atomic( this->tid_mask, 
	    this->thread_space_len );
    if( tidx >= this->thread_space_len )
	return L4_nilthread;

    return L4_GlobalId( this->thread_space_start + tidx, 2 );
}

void hthread_manager_t::thread_id_release( L4_ThreadId_t tid )
{
    static const word_t bits_per_word = sizeof(word_t)*8;
    L4_Word_t tidx = L4_ThreadNo(tid) - this->thread_space_start;
    bit_clear_atomic( tidx % bits_per_word,
	    this->tid_mask[tidx / bits_per_word] );
}

L4_Word_t hthread_manager_t::utcb_allocate()
{
    L4_Word_t uidx = bit_allocate_atomic( this->utcb_mask,
	    this->max_local_threads );
    if( uidx >= this->max_local_threads )
	return 0;

    return uidx*this->utcb_size + this->utcb_base;
}

void hthread_manager_t::utcb_release( L4_Word_t utcb )
{
    static const word_t bits_per_word = sizeof(word_t)*8;
    L4_Word_t uidx = (utcb - this->utcb_base)/utcb_size;
    ASSERT( uidx < this->max_local_threads );
    bit_clear_atomic( uidx % bits_per_word,
	    this->utcb_mask[uidx / bits_per_word] );
}

void NORETURN hthread_t::self_halt( void )
{
    while( 1 )
    {
	con << "Self-halting thread " << L4_Myself() << ".\n";
	L4_Stop( L4_MyLocalId() );
    }
}

void NORETURN hthread_t::self_start( void )
{
    // Go fishing for our information on the stack.
    hthread_t *hthread = (hthread_t *)L4_UserDefinedHandle();

    // Execute the functions.
    hthread->start_func( hthread->start_param, hthread );
    hthread_t::self_halt();
}

void hthread_manager_t::terminate_thread( L4_ThreadId_t tid )
{
    L4_ThreadId_t ltid, gtid;
    L4_Error_t errcode;

    if( L4_IsLocalId(tid) ) {
	ltid = tid;
	gtid = L4_GlobalIdOf( tid );
    } else {
	gtid = tid;
	ltid = L4_LocalIdOf( tid );
    }
    if( L4_IsNilThread(ltid) || L4_IsNilThread(gtid) )
	return;

    errcode = 
	ThreadControl( tid, L4_nilthread, L4_nilthread, L4_nilthread, ~0UL );
    if( errcode == L4_ErrOk ) {
	utcb_release( ltid.raw );
	thread_id_release( gtid );
    }
    else
	con << "Error: unable to delete the L4 thread " << gtid 
	    << ", L4 error: " << L4_ErrString(errcode) << ".\n";
}




thread_info_t *allocate_thread()
{
    L4_Error_t errcode;
    vcpu_t &vcpu = get_vcpu();
    L4_ThreadId_t controller_tid = vcpu.main_gtid;

    // Allocate a thread ID.
    L4_ThreadId_t tid = get_hthread_manager()->thread_id_allocate();
    if( L4_IsNilThread(tid) )
	PANIC( "Out of thread ID's." );

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
	errcode = ThreadControl( tid, task_info->get_space_tid(), 
		controller_tid, L4_nilthread, utcb );
	if( errcode != L4_ErrOk )
	    PANIC( "Failed to create initial user thread, TID " << tid 
		    << ", L4 error: " << L4_ErrString(errcode) );

	// Create an L4 address space + thread.
	// TODO: don't hardcode the size of a utcb to 512-bytes
	L4_Fpage_t utcb_fp = L4_Fpage( user_vaddr_end,
		512*CONFIG_L4_MAX_THREADS_PER_TASK );
	L4_Fpage_t kip_fp = L4_Fpage( L4_Address(utcb_fp) + L4_Size(utcb_fp),
		KB(16) );

	errcode = SpaceControl( tid, 0, kip_fp, utcb_fp, 
		L4_nilthread );
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

    // Set the thread priority.
    L4_Word_t prio = vcpu.get_vm_max_prio() + CONFIG_PRIO_DELTA_USER;
    if( !L4_Set_Priority(tid, prio) )
	PANIC( "Failed to set user thread's priority to " << prio );

    // Assign the thread info to the guest OS's thread.
    afterburn_thread_assign_handle( thread_info );

    return thread_info;
}

void delete_thread( thread_info_t *thread_info )
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
	ThreadControl( tid, L4_nilthread, L4_nilthread, L4_nilthread, ~0UL );
	get_hthread_manager()->thread_id_release( tid );

	// Release the task info structure.
	task_manager_t::get_task_manager().deallocate( thread_info->ti );
    }

    // Release the thread info structure.
    thread_manager_t::get_thread_manager().deallocate( thread_info );
}
