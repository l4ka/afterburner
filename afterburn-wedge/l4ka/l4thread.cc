/*********************************************************************
 *
 * Copyright (C) 2005, University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4-common/l4thread.c
 * Description:   The l4thread library; provides simple wrappers for
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
 * $Id: l4thread.cc,v 1.11 2005/08/26 15:43:47 joshua Exp $
 *
 ********************************************************************/
#include <string.h>
#include <l4/thread.h>
#include <l4/schedule.h>
#include <l4/kip.h>

#include <console.h>
#include <debug.h>
#include INC_ARCH(bitops.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(l4thread.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(message.h)

l4thread_manager_t l4thread_manager;
void l4thread_t::arch_prepare_start(  )
{
    start_ip = (L4_Word_t)l4thread_t::self_start;
}


/* l4thread_manager_t Implementation.
 */
void l4thread_manager_t::init( L4_Word_t tid_space_start, L4_Word_t tid_space_len )
{
    // Zero the masks.
    L4_Word_t i;
    for( i = 0; i < sizeof(utcb_mask)/sizeof(utcb_mask[0]); i++ )
	utcb_mask[i] = 0;
    for( i = 0; i < sizeof(tid_mask)/sizeof(tid_mask[0]); i++ )
	tid_mask[i] = 0;

    this->thread_space_start = tid_space_start;
    this->thread_space_len = tid_space_len;
    if( this->thread_space_len > l4thread_manager_t::max_threads )
	this->thread_space_len = l4thread_manager_t::max_threads;

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

l4thread_t * l4thread_manager_t::create_thread( 
    vcpu_t *vcpu,
    L4_Word_t stack_bottom,
    L4_Word_t stack_size,
    L4_Word_t prio,
    l4thread_func_t start_func,
    L4_ThreadId_t pager_tid,
    void *start_param,
    void *tlocal_data,
    L4_Word_t tlocal_size)
{
    L4_Error_t errcode;
    
    ASSERT(vcpu->is_valid_vcpu());

    if (L4_Myself() != get_vcpu().monitor_gtid)
    {
	l4thread_t *ret;
	msg_thread_create_build(vcpu, stack_bottom, stack_size, prio, (void *) start_func, 
				pager_tid, start_param, tlocal_data, tlocal_size);
	
	L4_Call(get_vcpu().monitor_gtid); 
	msg_thread_create_done_extract((void **) &ret);
	return ret;
    }
    
    if( tlocal_size > stack_size/2 )
    {
	printf( "Error: stack size is too small for the thread local data.\n");
	return NULL;
    }

    L4_ThreadId_t tid = this->thread_id_allocate();
    if( L4_IsNilThread(tid) ) {
	printf( "Error: out of thread ID's.\n");
	return NULL;
    }

    // Is the thread already running?
    L4_Word_t result;
    L4_ThreadId_t local_tid, dummy_tid;
    local_tid = L4_ExchangeRegisters( tid, 0, 0, 0, 0, 0, L4_nilthread, 
				      &result, &result, &result, &result, &result, &dummy_tid );
    if( !L4_IsNilThread(local_tid) ) {
	printf( "Error: attempt to recreate a running thread.\n");
	this->thread_id_release( tid );
	return NULL;
    }

    // Create the thread.
    L4_Word_t utcb = this->utcb_allocate();
    if( !utcb ) {
	printf( "Error: out of UTCB space.\n");
	this->thread_id_release( tid );
	return NULL;
    }
    
    errcode = ThreadControl( tid, L4_Myself(), vcpu->monitor_gtid, pager_tid, utcb, prio );
    if( errcode != L4_ErrOk ) {
	printf( "Error: unable to create a thread, L4 error: %d\n", L4_ErrString(errcode));
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
	printf("Error: unable to set thread %t's prio to %d, processor number to %d"
	       ", timeslice/quantum to %d, L4 error: %d\n", 
	       tid, priority, vcpu->get_pcpu_id(), time_control, L4_ErrString(L4_ErrorCode()));
	this->thread_id_release( tid );
	return NULL;
    }

    // Create the thread's stack.
    l4thread_t *l4thread = (l4thread_t *) (stack_bottom + stack_size - sizeof(l4thread_t));
    l4thread->start_sp = stack_bottom + stack_size - sizeof(l4thread_t);
    l4thread->tlocal_data = NULL;
    l4thread->start_func = start_func;
    l4thread->start_param = start_param;
    l4thread->start_ip = NULL;
    if( tlocal_data != NULL )
    {
	// Put the thread local data on the stack.
	l4thread->start_sp -= tlocal_size;
	l4thread->tlocal_data = (void *) l4thread->start_sp;
	memcpy( l4thread->tlocal_data, tlocal_data, tlocal_size );
    }

    // Ensure that the stack conforms to the function calling ABI.
    l4thread->start_sp = (l4thread->start_sp - CONFIG_STACK_SAFETY) & ~(CONFIG_STACK_ALIGN-1);

    // Let architecture-specific code prepare for the thread-start trampoline.
    l4thread->arch_prepare_start();

    L4_Word_t exregs_flags = (3 << 3) | (1 << 6);    
#if defined(CONFIG_L4KA_VMEXT)
    {
	/* Set the thread's exception handler and configure cxfer messages via
	 * exregs */
	exregs_flags |= L4_EXREGS_EXCHANDLER_FLAG | L4_EXREGS_CTRLXFER_CONF_FLAG;
	/* Set exception ctrlxfer mask */
	L4_Msg_t ctrlxfer_msg;
	L4_Word64_t fault_id_mask = (1<<2) | (1<<3) | (1<<5);
	L4_Word_t fault_mask = L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_GPREGS_ID);
	L4_Clear(&ctrlxfer_msg);
	L4_MsgAppendWord (&ctrlxfer_msg, vcpu->monitor_gtid.raw);
	L4_AppendFaultConfCtrlXferItems(&ctrlxfer_msg, fault_id_mask, fault_mask);
	L4_Load(&ctrlxfer_msg);
    }	
#endif
    
    // Set the thread's starting SP and starting IP.
    local_tid = L4_ExchangeRegisters( tid, exregs_flags, 
				      l4thread->start_sp, l4thread->start_ip, 0, L4_Word_t(l4thread),
				      L4_nilthread, &result, &result, &result, &result, &result,
				      &dummy_tid );
    
    if( L4_IsNilThread(local_tid) )
    {	
	printf( "Error: unable to setup thread %t, L4 error: %d\n", 
		tid, L4_ErrString(L4_ErrorCode()));
	this->thread_id_release( tid );
	return NULL;
    }

    l4thread->local_tid = local_tid;


    UNUSED bool mbt = get_vcpu().add_vcpu_thread(tid, local_tid);
    ASSERT(mbt);
    return l4thread;
}

L4_ThreadId_t l4thread_manager_t::thread_id_allocate()
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

void l4thread_manager_t::thread_id_release( L4_ThreadId_t tid )
{
    static const word_t bits_per_word = sizeof(word_t)*8;
    L4_Word_t tidx = L4_ThreadNo(tid) - this->thread_space_start;
    bit_clear_atomic( tidx % bits_per_word,
	    this->tid_mask[tidx / bits_per_word] );
}

L4_Word_t l4thread_manager_t::utcb_allocate()
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

void l4thread_manager_t::utcb_release( L4_Word_t utcb )
{
    static const word_t bits_per_word = sizeof(word_t)*8;
    L4_Word_t uidx = (utcb - this->utcb_base)/utcb_size;
    ASSERT( uidx < this->max_local_threads );
    bit_clear_atomic( uidx % bits_per_word,
	    this->utcb_mask[uidx / bits_per_word] );
}

void NORETURN l4thread_t::self_halt( void )
{
    while( 1 )
    {
	printf( "Error: self halting thread %t\n", L4_Myself());
	L4_Stop( L4_MyLocalId() );
    }
}

void NORETURN l4thread_t::self_start( void )
{
    // Go fishing for our information on the stack.
    l4thread_t *l4thread = (l4thread_t *)L4_UserDefinedHandle();

    // Execute the functions.
    l4thread->start_func( l4thread->start_param, l4thread );
    l4thread_t::self_halt();
}

void l4thread_manager_t::terminate_thread( L4_ThreadId_t tid )
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

    UNUSED bool mbt = get_vcpu().remove_vcpu_thread(tid);
    ASSERT(mbt);

    errcode = 
	ThreadControl( tid, L4_nilthread, L4_nilthread, L4_nilthread, ~0UL );
    if( errcode == L4_ErrOk ) {
	utcb_release( ltid.raw );
	thread_id_release( gtid );
    }
    else
	printf( "Error: unable to delete thread %t, L4 error: %d\n", 
		gtid, L4_ErrString(L4_ErrorCode()));
    
}

