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
#include <memory.h>
#include <l4/thread.h>
#include <l4/schedule.h>
#include <l4/kip.h>

#include INC_ARCH(bitops.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(message.h)

hthread_manager_t hthread_manager;

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

    L4_KernelInterfacePage_t *kip = (L4_KernelInterfacePage_t *) L4_GetKernelInterface();
    this->utcb_size = L4_UtcbSize( kip );
    this->utcb_base = L4_MyLocalId().raw & ~(utcb_size - 1);
    // Adjust to correlate with thread_space_start.
    this->utcb_base -= my_tidx*this->utcb_size;

    // Update the UTCB bitmask to reflect our currently running thread.
    L4_Word_t my_uidx = (L4_MyLocalId().raw - this->utcb_base)/this->utcb_size;
    bit_set_atomic( my_uidx % sizeof(word_t), 
	    this->utcb_mask[my_uidx / sizeof(word_t)] );
    
#if defined(CONFIG_VSMP)
    thread_mgmt_lock.init("tmgr");
#endif

}

hthread_t * hthread_manager_t::create_thread( 
    vcpu_t *vcpu,
    L4_Word_t stack_bottom,
    L4_Word_t stack_size,
    L4_Word_t prio,
    hthread_func_t start_func,
    L4_ThreadId_t pager_tid,
    void *start_param,
    void *tlocal_data,
    L4_Word_t tlocal_size)
{
    L4_Error_t errcode;
    
    ASSERT(vcpu->is_valid_vcpu());

    if (L4_Myself() != get_vcpu().monitor_gtid)
    {
	hthread_t *ret;
	msg_thread_create_build(vcpu, stack_bottom, stack_size, prio, (void *) start_func, 
		pager_tid, start_param, tlocal_data, tlocal_size);
	
	L4_Call(get_vcpu().monitor_gtid); 
	msg_thread_create_done_extract((void **) &ret);
	return ret;
    }
    
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
    
    errcode = ThreadControl( tid, L4_Myself(), vcpu->monitor_gtid, pager_tid, utcb, prio );
    if( errcode != L4_ErrOk ) {
	con << "Error: unable to create a thread, L4 error: " 
	    << L4_ErrString(errcode) << ".\n";
	this->thread_id_release( tid );
	return NULL;
    }
    
    // Set the thread priority, timeslice, etc.
    L4_Word_t preemption_control = ~0UL;
    L4_Word_t time_control = ~0UL;
    L4_Word_t priority = prio;
    L4_Word_t processor_control = vcpu->get_pcpu_id() & 0xffff;
    L4_Word_t dummy;
    

    if (!L4_Schedule(tid, time_control, processor_control, priority, preemption_control, &dummy))
    {
	con << "Error: unable to set thread " << tid << " priority to " << prio 	
	    << " or to set user thread's processor number to " << vcpu->get_pcpu_id()
	    << " or to set user thread's timeslice/quantum to " << (void *) time_control
	    << "ErrCode " << L4_ErrString(L4_ErrorCode())
	    << "\n";
	this->thread_id_release( tid );
	return NULL;
    }

    // Create the thread's stack.
    hthread_t *hthread = (hthread_t *) (stack_bottom + stack_size - sizeof(hthread_t));
    hthread->start_sp = stack_bottom + stack_size - sizeof(hthread_t);
    hthread->tlocal_data = NULL;
    hthread->start_func = start_func;
    hthread->start_param = start_param;
    hthread->start_ip = NULL;
    if( tlocal_data != NULL )
    {
	// Put the thread local data on the stack.
	hthread->start_sp -= tlocal_size;
	hthread->tlocal_data = (void *) hthread->start_sp;
	memcpy( hthread->tlocal_data, tlocal_data, tlocal_size );
    }

    // Ensure that the stack conforms to the function calling ABI.
    hthread->start_sp = (hthread->start_sp - CONFIG_STACK_SAFETY) & ~(CONFIG_STACK_ALIGN-1);

    // Let architecture-specific code prepare for the thread-start trampoline.
    hthread->arch_prepare_start();

    // Set the thread's starting SP and starting IP.
    local_tid = L4_ExchangeRegisters( tid, (3 << 3) | (1 << 6), 
	    hthread->start_sp, hthread->start_ip, 0, L4_Word_t(hthread),
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

    bool mbt = get_vcpu().add_vcpu_hthread(tid);
    ASSERT(mbt);
    
    return hthread;
}

L4_ThreadId_t hthread_manager_t::thread_id_allocate()
{
    L4_ThreadId_t ret;
    
    L4_Word_t tidx = bit_allocate_atomic( this->tid_mask, 
	    this->thread_space_len );
    if( tidx >= this->thread_space_len )
	ret = L4_nilthread;
    else
	ret = L4_GlobalId( this->thread_space_start + tidx, 2 );
    
    return ret;
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
    L4_Word_t ret;
    
    L4_Word_t uidx = bit_allocate_atomic( this->utcb_mask,
	    this->max_local_threads );
    if( uidx >= this->max_local_threads )
	ret = 0;
    else
	ret = uidx*this->utcb_size + this->utcb_base;
    
    return ret;
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

