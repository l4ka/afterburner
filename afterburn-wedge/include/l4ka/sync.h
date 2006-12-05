/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/l4ka/vcpu.h
 * Description:   The per-CPU data declarations for the L4Ka environment.
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
 * $Id: vcpu.h,v 1.19 2006/01/04 15:16:24 joshua Exp $
 *
 ********************************************************************/
#ifndef __L4KA__SYNC_H__
#define __L4KA__SYNC_H__

#include <l4/kip.h>
#include <l4/thread.h>
#include <l4/kdebug.h>
#include <l4/ipc.h>
#include INC_ARCH(sync.h)

#undef L4KA_DEBUG_SYNC
#if defined(L4KA_DEBUG_SYNC)
#define LOCK_DEBUG(cpu, c)					\
do {								\
    L4_KDB_PrintChar((char) cpu + '0');				\
    L4_KDB_PrintChar(c);					\
 } while (0)
#else
#define LOCK_DEBUG(cpu, c)
#endif

#define L4KA_ASSERT_SYNC

#if defined(L4KA_ASSERT_SYNC)
#define LOCK_ASSERT(x, c)					\
    do {							\
	if(EXPECT_FALSE(!(x))) {				\
	    L4_KDB_PrintChar('\n');L4_KDB_PrintChar('L');	\
	    L4_KDB_PrintChar('O');L4_KDB_PrintChar('C');	\
	    L4_KDB_PrintChar('K');L4_KDB_PrintChar(' ');	\
	    L4_KDB_PrintChar('A');L4_KDB_PrintChar('S');	\
	    L4_KDB_PrintChar('S');L4_KDB_PrintChar('E');	\
	    L4_KDB_PrintChar('R');L4_KDB_PrintChar('T');	\
	    L4_KDB_PrintChar(' ');L4_KDB_PrintChar('F');	\
	    L4_KDB_PrintChar('A');L4_KDB_PrintChar('I');	\
	    L4_KDB_PrintChar('L');L4_KDB_PrintChar('E');	\
	    L4_KDB_PrintChar('D');L4_KDB_PrintChar('(');	\
	    L4_KDB_PrintChar(c);L4_KDB_PrintChar(')');		\
	    L4_KDB_PrintChar('\n');				\
	    L4_KDB_Enter("Failed Lock Assertion");		\
	    while (1) ;						\
	}							\
    } while(0)							
#else 
#define LOCK_ASSERT(x, c)
#endif

class trylock_t
{
public:
    union
    {
	struct 
	{
	    u32_t		owner_thread_version	:  7;
	    u32_t		owner_pcpu_id		:  7;
	    u32_t		owner_thread_no		: 18;
	} x;
	u32_t raw;
    } lock;	
    
    word_t get_raw()
	{ return lock.raw; }
    void set_raw_atomic(word_t val)
	{  
	    __asm__ __volatile__ (
		"movl %0, %1"
		: /* no output */
		: "r"(val), "m"(lock.raw)
	    );
	}
    L4_ThreadId_t get_owner_tid()
	{ 
	    L4_ThreadId_t ret = { raw : 0};
	    ret.global.X.thread_no = lock.x.owner_thread_no;
	    ret.global.X.version = lock.x.owner_thread_version;
	    return ret;	
	}
    word_t get_owner_pcpu_id()
	{ return lock.x.owner_pcpu_id; }
    
    void set_owner_tid(L4_ThreadId_t tid)
	{ 
	    LOCK_ASSERT(tid.global.X.version < 128, 'V');
	    lock.x.owner_thread_no = tid.global.X.thread_no; 
	    lock.x.owner_thread_version = tid.global.X.version; 
	}
    void set_owner_pcpu_id(word_t pcpu_id)
	{ lock.x.owner_pcpu_id = pcpu_id;}
    
};

class cpu_lock_t
{
private:
    trylock_t cpulock;
    
    bool trylock(L4_ThreadId_t tid, word_t pcpu_id, L4_ThreadId_t *old_tid, word_t *old_pcpu_id )
	{
	    bool ret;
	    
	    trylock_t new_lock;
	    new_lock.set_owner_pcpu_id(pcpu_id);
	    new_lock.set_owner_tid(tid);

	    trylock_t empty_lock;
	    empty_lock.set_owner_pcpu_id(max_pcpus);
	    empty_lock.set_owner_tid(L4_nilthread);

	    ret = cmpxchg_ext(&cpulock, empty_lock, &new_lock);
	    
	    *old_pcpu_id = new_lock.get_owner_pcpu_id();
	    *old_tid = new_lock.get_owner_tid();
	    return ret;
	}

    void release()
	{ 
	    trylock_t new_lock_val;
	    new_lock_val.set_owner_tid(L4_nilthread);
	    new_lock_val.set_owner_pcpu_id(max_pcpus);
	    cpulock.set_raw_atomic(new_lock_val.get_raw());
	}
    
    
public:
    static word_t	max_pcpus;
#if defined(L4KA_DEBUG_SYNC)
    static L4_Word_t debug_pcpu_id;
    static L4_ThreadId_t debug_tid;
    static cpu_lock_t *debug_lock;
#endif
    void init()
	{ 
	    max_pcpus = L4_NumProcessors(L4_GetKernelInterface());
    	    cpulock.set_owner_pcpu_id(max_pcpus);
	    cpulock.set_owner_tid(L4_nilthread);
	    LOCK_ASSERT(sizeof(cpu_lock_t) == 4, '0');
	}

    
    bool is_locked_by_tid(L4_ThreadId_t owner)
	{
	    return (cpulock.get_owner_tid() == owner);
	}
    
    bool is_locked_by_cpu(L4_Word_t pcpu_id)
	{
	    return (cpulock.get_owner_pcpu_id() == pcpu_id);
	}
    
    bool is_locked()
	{
	    return (cpulock.get_owner_pcpu_id() != max_pcpus);
	}

    void lock()
	{
	    /*
	     * Logic: 
	     *  owner_pcpu_id == CONFIG_NR_VCPUS : lock is free -> get it
	     *  owner_pcpu_id == my_vcpu_id : lock held by my VCPU -> donate time
	     *  owner_pcpu_id == pcpu_id : lock held by VCPU vcpu_id -> spin
	     * 
	     */
	restart:
	    
	    word_t new_pcpu_id = L4_ProcessorNo();
	    L4_ThreadId_t new_tid = L4_Myself();
	    
	    word_t old_pcpu_id = max_pcpus;
	    L4_ThreadId_t old_tid = L4_nilthread;
	    

	    
	    if (!trylock(new_tid, new_pcpu_id, &old_tid, &old_pcpu_id))
	    {
#if defined(L4KA_DEBUG_SYNC)
		debug_tid = old_tid;
		debug_pcpu_id = old_pcpu_id;
#endif
		LOCK_ASSERT(old_tid != L4_nilthread, '1');
		LOCK_ASSERT(cpulock.get_owner_tid() != L4_Myself(), '2');
		
		if (old_pcpu_id == new_pcpu_id)
		{
		    LOCK_DEBUG(new_pcpu_id, 'p');
		    L4_ThreadSwitch(old_tid);
		}
		else 
		{
		    LOCK_DEBUG(new_pcpu_id, 'q');
		    while (is_locked_by_cpu(old_pcpu_id))
			memory_barrier();
		}
		goto restart;
	    }
	    else 
	    {
		LOCK_ASSERT(old_pcpu_id == max_pcpus, '3');
		LOCK_ASSERT(old_tid == L4_nilthread, '4');
	    }
	    
	    //LOCK_DEBUG(new_pcpu_id, '!');
#if defined(L4KA_DEBUG_SYNC)
	    debug_tid = cpulock.get_owner_tid();
	    debug_pcpu_id = cpulock.get_owner_pcpu_id();
	    debug_lock = this;
#endif		
	    
	}
    
    void unlock()
	{
	    LOCK_ASSERT(cpulock.get_owner_tid() == L4_Myself(), 'a');
	    LOCK_ASSERT(cpulock.get_owner_pcpu_id() == (word_t) L4_ProcessorNo(), 'b');
	    //LOCK_DEBUG(cpulock.get_owner_pcpu_id(), '^');
	    release();
	}
    

};


#endif /* !__L4KA__SYNC_H__ */
