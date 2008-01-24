/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/include/hthread.h
 * Description:   Support for threading within the resourcemon.
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
#ifndef __HYPERVISOR__INCLUDE__HTHREAD_H__
#define __HYPERVISOR__INCLUDE__HTHREAD_H__

#include <l4/thread.h>
#include <resourcemon/resourcemon.h>
#include <common/debug.h>

extern char __L4_syscalls_start;
extern char __L4_syscalls_end;
extern "C" void __L4_copy_syscalls_in (L4_Word_t dest);

/* Configuration.
 */
#define RESOURCEMON_STACK_SIZE	KB(32)

#if defined(__i386__)
#define ARCH_STACK_ALIGN	(16UL)
#define ARCH_STACK_SAFETY	(4UL)

#if 0
#elif defined(CONFIG_ARCH_IA64)
#define ARCH_STACK_ALIGN	(16UL)
#define ARCH_STACK_SAFETY	(16UL)
#elif defined(CONFIG_ARCH_POWERPC)
#define ARCH_STACK_ALIGN	(16UL)
#define ARCH_STACK_SAFETY	(16UL)
#endif

#endif

#if !defined(ASSEMBLY)

typedef enum hthread_idx_e {
    hthread_idx_main=0,
#if defined(CONFIG_PERFMON_SCAN)
    hthread_idx_perfmon_scan,
#endif
    hthread_idx_working_set,
#if defined(cfg_l4ka_vmextensions)
    hthread_idx_virq,
    hthread_idx_virq_end = hthread_idx_virq + IResourcemon_max_cpus,
#endif
#if defined(cfg_earm)
    hthread_idx_earm_accmanager,
    hthread_idx_earm_accmanager_debug,
    hthread_idx_earm_acccpu,
    hthread_idx_earm_acccpu_col,
    hthread_idx_earm_easmanager,
    hthread_idx_earm_easmanager_throttle,
    hthread_idx_earm_easmanager_control,
#endif
    hthread_idx_max
};

class hthread_t;
typedef void (*hthread_func_t)( void *, hthread_t * );


/* Thread support data.
 * Do not edit this! Used in architecture dependent assembly
 */
class hthread_t
{
private:
    L4_ThreadId_t local_tid;
    L4_ThreadId_t global_tid;
    L4_Word_t stack_memory;
    L4_Word_t stack_size;

    void *tlocal_data;

    void *start_param;
    hthread_func_t start_func;
    


    void arch_prepare_exreg( L4_Word_t &sp, L4_Word_t &ip );

    static void self_halt();
    static void self_start();

public:
    friend class hthread_manager_t;

    L4_ThreadId_t get_local_tid()
	{ return this->local_tid; }
    L4_ThreadId_t get_global_tid()
	{ return this->global_tid; }

    void start()
	{ 
	    L4_Start( this->get_local_tid() ); 
	}
};


class hthread_manager_t
{
private:
    L4_ThreadId_t base_tid;
    L4_Word_t utcb_size;
    L4_Word_t utcb_base;
    L4_Word_t smallspace_base;
    static const L4_Word_t smallspace_size = 16;
    static const L4_Word_t smallspace_area_size = 256;
    L4_Word_t syscall_stubs[4096] __attribute__ ((aligned (4096)));

public:
    void init();
    hthread_t * create_thread( hthread_idx_e tidx, L4_Word_t prio, bool small_space,
			       hthread_func_t start_func, void *param=NULL, 
			       void *tlocal_data=NULL, L4_Word_t tlocal_size=0, 
			       L4_Word_t domain=0);

    bool resolve_hthread_pfault (L4_ThreadId_t tid, L4_Word_t addr, L4_Word_t ip, L4_Word_t &haddr) 
	{
	
	    if (!is_hthread(tid))
		return false;
	    
	    if (addr >= (L4_Word_t) &__L4_syscalls_start &&
		addr <  (L4_Word_t) &__L4_syscalls_end)
	    {
		__L4_copy_syscalls_in ((L4_Word_t) syscall_stubs);
		haddr = (L4_Word_t) syscall_stubs;

	    }
	    else
		haddr = addr;
	    
	    return true;
	    
    }
    bool is_hthread(L4_ThreadId_t tid)
	{
	    if (L4_IsLocalId(tid))
		return true;
	    
	    L4_Word_t idx = tid.global.X.thread_no - this->base_tid.global.X.thread_no;
	    
	    if (idx > hthread_idx_max)
		return false;
	    
	    if (tid.global.X.thread_no - idx != base_tid.global.X.thread_no)
		return false;
	    
	    return (tid.global.X.version == 2);	    
	}
    L4_Word_t tid_to_idx( L4_ThreadId_t tid )
    {
	return tid.global.X.thread_no - this->base_tid.global.X.thread_no;
    }
};

extern inline hthread_manager_t * get_hthread_manager()
{
    extern hthread_manager_t hthread_manager;
    return &hthread_manager;
}

#endif  /* ASSEMBLY */
#endif	/* __HYPERVISOR__INCLUDE__HTHREAD_H__ */
