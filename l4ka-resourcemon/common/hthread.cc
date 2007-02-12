/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:	resourcemon/hthread.cc
 * Description:	Management of threads internal to the resourcemon.
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

#include <l4/types.h>
#include <l4/thread.h>
#include <l4/schedule.h>
#include <l4/kip.h>

#include <common/hthread.h>
#include <common/console.h>
#include <common/string.h>
#if defined(cfg_logging)
#include <resourcemon/logging.h>
#include <resourcemon/vm.h>
#endif
#define RESOURCEMON_STACK_BOTTOM(tidx)	\
	((L4_Word_t)&resourcemon_thread_stacks[(tidx)][0])

static unsigned char __attribute__((aligned(ARCH_STACK_ALIGN)))
	resourcemon_thread_stacks[hthread_idx_max][RESOURCEMON_STACK_SIZE];


L4_Word_t resourcemon_first_stack = 
    (L4_Word_t(&resourcemon_thread_stacks[0][RESOURCEMON_STACK_SIZE]) - 
     sizeof(hthread_t) - ARCH_STACK_SAFETY);

hthread_manager_t hthread_manager;


/* hthread_manager_t Implementation.
 */
void hthread_manager_t::init()
{
    // Note: the first stack is already in use by our current thread.
    this->base_tid = L4_Myself();
    this->utcb_size = L4_UtcbSize( L4_GetKernelInterface() );
    this->utcb_base = L4_MyLocalId().raw & ~(utcb_size - 1);
}

hthread_t * hthread_manager_t::create_thread( 
	hthread_idx_e tidx,
	L4_Word_t prio,
	hthread_func_t start_func,
	void *start_param,
	void *tlocal_data,
	L4_Word_t tlocal_size,
	L4_Word_t domain)
{
    if( tidx >= hthread_idx_max )
    {
	hout << "Error: attempt to create too many resourcemon threads.\n";
	return NULL;
    }

    if( tlocal_size > RESOURCEMON_STACK_SIZE/2 )
    {
	hout << "Error: thread local data is too large.\n";
	return NULL;
    }

    L4_ThreadId_t tid;
    tid.global.X.thread_no = this->base_tid.global.X.thread_no + tidx;
    tid.global.X.version = 2;

    // Is the thread already running?
    L4_Word_t result;
    L4_ThreadId_t local_tid, dummy_tid;
    local_tid = L4_ExchangeRegisters( tid, 0, 0, 0, 0, 0, L4_nilthread, 
	    &result, &result, &result, &result, &result, &dummy_tid );
    if( !L4_IsNilThread(local_tid) )
    {
	hout << "Error: attempt to recreate a running thread.\n";
	return NULL;
    }

    // Create the thread.
    L4_Word_t utcb = this->utcb_base + tidx*this->utcb_size;
#if defined(cfg_logging) 
    L4_Word_t d = (domain) ? domain : L4_LOGGING_ROOTTASK_DOMAIN;

    result = L4_ThreadControlDomain( tid, base_tid, base_tid, base_tid,
			       (void *)utcb, d);

    vm_t::propagate_max_domain_in_use(domain);
#else
   result = L4_ThreadControl( tid, base_tid, base_tid, base_tid,
	    (void *)utcb );
#endif
   if( !result )
    {
	hout << "Error: unable to create a thread, L4 error code: "
	     << L4_ErrorCode() << '\n';
	return NULL;
    }

    // Priority
    if( !L4_Set_Priority(tid, prio) )
    {
	hout << "Error: unable to set a thread's priority to " << prio
	     << ", L4 error code: " << L4_ErrorCode() << '\n';
	return NULL;
    }

    // Create the thread's stack.
    L4_Word_t sp, ip;
    sp = L4_Word_t(&resourcemon_thread_stacks[tidx][RESOURCEMON_STACK_SIZE]);
    sp -= sizeof(hthread_t);
    hthread_t *hthread = (hthread_t *)sp;
    hthread->tlocal_data = NULL;
    hthread->start_func = start_func;
    hthread->start_param = start_param;
    hthread->stack_memory = RESOURCEMON_STACK_BOTTOM(tidx);
    hthread->stack_size = RESOURCEMON_STACK_SIZE;

    if( tlocal_data != NULL )
    {
	// Put the thread local data on the stack.
	sp -= tlocal_size;
	hthread->tlocal_data = (void *)sp;
	memcpy( hthread->tlocal_data, tlocal_data, tlocal_size );
    }

    // Ensure that the stack conforms to the function calling ABI.
    sp = (sp - ARCH_STACK_SAFETY) & ~(ARCH_STACK_ALIGN-1);

    // Let architecture-specific code prepare for the thread-start trampoline.
    hthread->arch_prepare_exreg( sp, ip );

    
    // Set the thread's starting SP and starting IP.
    local_tid = L4_ExchangeRegisters( tid, (3 << 3) | (1 << 6), 
	    sp, ip, 0, L4_Word_t(hthread),
	    L4_nilthread, &result, &result, &result, &result, &result,
	    &dummy_tid );
    if( L4_IsNilThread(local_tid) )
    {
	hout << "Error: unable to setup a thread, L4 error code: "
	     << L4_ErrorCode() << '\n';
	return NULL;
    }
    

    hthread->local_tid = local_tid;
    hthread->global_tid = L4_GlobalId(local_tid);
    return hthread;
}

void __noreturn hthread_t::self_halt( void )
{
    while( 1 )
    {
	hout << "Self-halting thread " << L4_Myself() << ".\n";
	L4_Stop( L4_MyLocalId() );
    }
}

void __noreturn hthread_t::self_start( void )
{
    // Go fishing for our information on the stack.
    hthread_t *hthread = (hthread_t *)L4_UserDefinedHandle();

    // Execute the functions.
    hthread->start_func( hthread->start_param, hthread );
    hthread_t::self_halt();

}

