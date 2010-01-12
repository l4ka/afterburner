/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:	hthread.cc
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

#include <basics.h>
#include <hthread.h>
#include <string.h>
#include <vm.h>
#include <virq.h>
#include <logging.h>
#include <earm.h>


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
    this->utcb_size = L4_UtcbSize( l4_kip );
    this->utcb_base = L4_MyLocalId().raw & ~(utcb_size - 1);
    this->smallspace_base = 0;
}

hthread_t * hthread_manager_t::create_thread( 
    hthread_idx_e tidx,
    L4_Word_t prio,
    bool small_space,
    hthread_func_t start_func,
    void *start_param,
    void *tlocal_data,
    L4_Word_t tlocal_size,
    L4_Word_t logid)
{
    
    if (small_space && !l4_smallspaces_enabled)
    {
	printf("Small space hthread requested but L4 doesn't support small spaces\n");
	small_space = false;
    }	   
    if( tidx >= hthread_idx_max )
    {
	printf("Error: attempt to create too many resourcemon threads.\n");
	return NULL;
    }

    if( tlocal_size > RESOURCEMON_STACK_SIZE/2 )
    {
	printf( "Error: thread local data is too large.\n");
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
	printf( "Error: attempt to recreate a running thread.\n");
	return NULL;
    }

    // Create the thread.
    L4_ThreadId_t space_spec = small_space ? tid  : base_tid;
    L4_ThreadId_t pager_tid = small_space ? L4_nilthread  : base_tid;
    L4_ThreadId_t scheduler_tid = pager_tid;
    
    if (l4_pmsched_enabled && virqs[L4_ProcessorNo()].thread)
	scheduler_tid = virqs[L4_ProcessorNo()].thread->get_global_tid();
    
    L4_Word_t ubase = small_space ? 0 : this->utcb_base;
    L4_Word_t utcb = ubase + tidx*this->utcb_size;
    
    result = L4_ThreadControl( tid, space_spec, scheduler_tid, pager_tid, (void *)utcb );
    
    if( !result )
    {
	printf( "Error: unable to create a thread, L4 error: %d\n", 
		L4_ErrorCode_String(L4_ErrorCode()));
	L4_KDB_Enter("hthread BUG");
	return NULL;
    }
   
    if (small_space)
    {
        L4_Word_t dummy;
        L4_KernelInterfacePage_t * kip = (L4_KernelInterfacePage_t *) L4_KernelInterface ();
        L4_Fpage_t utcb_fp  = L4_Fpage(ubase, L4_UtcbAreaSize(kip));
        L4_Fpage_t kip_fp = L4_Fpage(L4_Address(utcb_fp) + L4_Size(utcb_fp), L4_KipAreaSize(kip));

        result = L4_SpaceControl (tid, 0, kip_fp, utcb_fp, L4_nilthread, &dummy);
        if( !result )
        {
            printf( "Error: unable to configure space, L4 error: %d\n", L4_ErrorCode_String(L4_ErrorCode()));
            L4_KDB_Enter("hthread BUG");
            return NULL;
        }
        result = L4_ThreadControl (tid, tid, scheduler_tid, base_tid, (void *) utcb); 
        if( !result )
        {
            printf( "Error: unable to configure thread, L4 error: %d\n", L4_ErrorCode_String(L4_ErrorCode()));
            L4_KDB_Enter("hthread BUG");
            return NULL;
        }
       
        ASSERT(smallspace_base < smallspace_area_size);
        L4_SpaceControl (tid, (1UL << 31) | L4_SmallSpace (smallspace_base, smallspace_size), 
                         L4_Nilpage, L4_Nilpage, L4_nilthread, &dummy);
        if( !result )
        {
            printf( "Error: unable to make space small, L4 error: %d\n", L4_ErrorCode_String(L4_ErrorCode()));
            L4_KDB_Enter("hthread BUG");
            return NULL;
        }
        smallspace_base += smallspace_size;
       
    }

   
    if (prio || l4_logging_enabled) 
    {
        L4_Word_t l = (logid) ? logid : L4_LOG_ROOTSERVER_LOGID;
        L4_Word_t prio_control = (prio & 0xff) | (l4_logging_enabled ? l << 9 : 0);
        L4_Word_t dummy;
            
        if (!L4_Schedule(tid, ~0UL, ~0UL, prio_control, ~0UL, &dummy))
        {
            printf( "Error: unable to set a thread's prio_control to %x, L4 error: ", prio_control, L4_ErrorCode());
            return NULL;
        }
        
        if (l4_logging_enabled)
            set_max_logid_in_use(logid);
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
	printf( "Error: unable to setup a thread, L4 error code: %d\n", L4_ErrorCode());
	return NULL;
    }
    
    if (l4_pmsched_enabled)
    {
	virq_t *virq = get_virq();
	
	if (virq->myself != L4_nilthread)
	{
	    setup_thread_faults(tid);
	    register_system_task(virq->mycpu, tid, local_tid, vm_state_blocked, false);
	}
    }


    hthread->local_tid = local_tid;
    if (small_space)
	hthread->global_tid = tid;
    else
	hthread->global_tid = L4_GlobalId(local_tid);
    return hthread;
}

void __noreturn hthread_t::self_halt( void )
{
    while( 1 )
    {
	printf( "Self-halting thread %x\n", L4_Myself());
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

