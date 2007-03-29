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

#include INC_WEDGE(debug.h)
#include INC_ARCH(sync.h)
#include INC_ARCH(bitops.h)
#include <templates.h>
#include <l4/kip.h>
#include <l4/thread.h>
#include <l4/schedule.h>
#include <l4/kdebug.h>
#include <l4/ipc.h>

class cpu_lock_t;

#if !defined(CONFIG_OPTIMIZE)
#define L4KA_DEBUG_SYNC
#define L4KA_ASSERT_SYNC
#endif

extern void ThreadSwitch(L4_ThreadId_t dest, cpu_lock_t *lock);

#if defined(L4KA_DEBUG_SYNC)

static inline void debug_hex_to_str( unsigned long val, char *s )
{
    static const char representation[] = "0123456789abcdef";
    bool started = false;
    for( int nibble = 2*sizeof(val) - 1; nibble >= 0; nibble-- )
    {
	unsigned data = (val >> (4*nibble)) & 0xf;
	if( !started && !data )
	    continue;
	started = true;
	*s++ = representation[data] ;
    }
}

#define LOCK_DEBUG(c, n, myself, mycpu, dst, dstcpu)		\
    do {							\
	extern char lock_debug_string[];			\
								\
	/* LOCK_DBG: C LCKN 12345678 1 12345678 1\n		\
	 * 0    5    10   15   20   25   30   35		\
	 */							\
	lock_debug_string[10]  = (char) c;			\
	lock_debug_string[12]  = n[0];				\
	lock_debug_string[13]  = n[1];				\
	lock_debug_string[14]  = n[2];				\
	lock_debug_string[15]  = n[3];				\
	debug_hex_to_str(myself.raw, &lock_debug_string[17]);	\
	lock_debug_string[26] = (char) mycpu + '0';		\
	debug_hex_to_str(dst.raw, &lock_debug_string[28]);	\
	lock_debug_string[37] = (char) dstcpu + '0';		\
	L4_KDB_PrintString(lock_debug_string);			\
								\
    } while (0)
#define DEBUG_COUNT_P	500000
#define DEBUG_COUNT_V	10

#else
#define LOCK_DEBUG(c, n, myself, mycpu, dst, dstcpu)		
#endif


#if defined(L4KA_ASSERT_SYNC)
#define LOCK_ASSERT(x, c, n)					\
    if(EXPECT_FALSE(!(x))) {					\
	extern char lock_assert_string[];			\
	lock_assert_string[12]  = n[0];				\
	lock_assert_string[13]  = n[1];				\
	lock_assert_string[14]  = n[2];				\
	lock_assert_string[15]  = n[3];				\
	lock_assert_string[16]  = ',';				\
	lock_assert_string[17]  = ' ';				\
	lock_assert_string[18]  = c;				\
	__asm__ __volatile__ (					\
	    "/* L4_KDB_Enter() */               \n\t"		\
	    "int     $3                         \n\t"		\
	    "jmp     2f                         \n\t"		\
	    "mov     $lock_assert_string, %eax  \n\t"		\
	    "2:                                 \n\t"		\
	    );}
#else 
#define LOCK_ASSERT(x, c, n)
#endif

class trylock_t
{
public:
    union
    {
#warning jsXXX: volatile trylock members
	struct 
	{
	    u32_t	owner_thread_version	:  7;
	    u32_t	owner_pcpu_id		:  7;
	    u32_t	owner_thread_no		: 18;
	} ;
	u32_t raw;
    } ;	
    
#if defined(L4KA_DEBUG_SYNC) || defined(L4KA_ASSERT_SYNC)
    const char *name;
#endif

    word_t get_raw()
	{ return raw; }
    
    void set_raw_atomic(word_t val)
	{  
	    __asm__ __volatile__ (
		"movl %0, %1"
		: /* no output */
		: "r"(val), "m"(raw)
	    );
	}
    
    L4_ThreadId_t get_owner_tid()
	{ 
	    L4_ThreadId_t ret = { raw : 0};
	    ret.global.X.thread_no = owner_thread_no;
	    ret.global.X.version = owner_thread_version;
	    return ret;	
	}
    
    word_t get_owner_pcpu_id()
	{ return owner_pcpu_id; }
    

    void set(L4_ThreadId_t owner_tid, word_t pcpu_id) 
	{ 
	    raw = 0;
	    LOCK_ASSERT(owner_tid.global.X.version < 128, '0', name);
	    owner_thread_no = owner_tid.global.X.thread_no; 
	    owner_thread_version = owner_tid.global.X.version; 
	    owner_pcpu_id = pcpu_id;
	} 


};

class cpu_lock_t
{
private:
    trylock_t cpulock;
   
    bool trylock(L4_ThreadId_t tid, word_t pcpu_id, L4_ThreadId_t *old_tid, word_t *old_pcpu_id )
	{
	    bool ret;
	    
	    trylock_t new_lock;
	    new_lock.set(tid, pcpu_id);
	    trylock_t empty_lock;
	    empty_lock.set(L4_nilthread, nr_pcpus);

	    ret = cmpxchg_ext(&cpulock.raw, empty_lock.raw, &new_lock.raw);
	    
	    *old_pcpu_id = new_lock.get_owner_pcpu_id();
	    *old_tid = new_lock.get_owner_tid();
	    return ret;
	}

    void release()
	{ 
	    trylock_t new_lock;
	    new_lock.set(L4_nilthread, nr_pcpus);
	    cpulock.set_raw_atomic(new_lock.get_raw());
	    if (bit_test_and_clear_atomic(0, delayed_preemption))
		L4_Yield();

	}
    
    
public:
    static word_t nr_pcpus;
    static bool delayed_preemption;
    
#if defined(L4KA_DEBUG_SYNC) || defined(L4KA_ASSERT_SYNC) 
    char *name() { return (char *) cpulock.name; }
#endif
	
    void init(const char *name = NULL);
    
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
	    return (cpulock.get_owner_pcpu_id() != nr_pcpus);
	}

    void lock( )
	{
#if defined(CONFIG_VSMP)
	    
	    /*
	     * Logic: 
	     *  owner_pcpu_id == CONFIG_NR_VCPUS : lock is free -> get it
	     *  owner_pcpu_id == my_vcpu_id : lock held by my VCPU -> donate time
	     *  owner_pcpu_id == pcpu_id : lock held by VCPU vcpu_id -> spin
	     * 
	     */
	    
	    word_t new_pcpu_id = L4_ProcessorNo();
	    L4_ThreadId_t myself = L4_Myself();
	    L4_ThreadId_t new_tid = myself;
	    word_t old_pcpu_id = nr_pcpus;
	    L4_ThreadId_t old_tid = L4_nilthread;
	    
#if defined(L4KA_DEBUG_SYNC)
	    L4_Word_t debug_count = 0;
	    bool debug  = false;
#endif
	    
	    while (!trylock(new_tid, new_pcpu_id, &old_tid, &old_pcpu_id))
	    {
		LOCK_ASSERT(old_pcpu_id != nr_pcpus, '2', name());
		LOCK_ASSERT(old_tid != L4_nilthread, '3', name());
		LOCK_ASSERT(cpulock.get_owner_tid() != myself, '4', name());

		if (old_pcpu_id == new_pcpu_id)
		{
		    
#if defined(L4KA_DEBUG_SYNC)
		    if (++debug_count == DEBUG_COUNT_V)
		    {
			LOCK_DEBUG('w', name(), myself, new_pcpu_id, old_tid, old_pcpu_id);
			debug_count = 0;
			debug = true;
		    }
#endif
		    ThreadSwitch(cpulock.get_owner_tid(), this );
		}
		else 
		{
		    while (is_locked_by_cpu(old_pcpu_id))
		    {
#if defined(L4KA_DEBUG_SYNC)
			if (++debug_count == DEBUG_COUNT_P)
			{
			    LOCK_DEBUG('p', name(), myself, new_pcpu_id, old_tid, old_pcpu_id);
			    debug_count = 0;
			    debug = true;
			}
#endif
			pause();
			memory_barrier();
		    }
		}
	    }
	    
#if defined(L4KA_DEBUG_SYNC)
	    if (debug)
		LOCK_DEBUG('l', name(), myself, new_pcpu_id, old_tid, old_pcpu_id);
#endif
	    
#endif /* CONFIG_VSMP */
	    
	}
    
    void unlock()
	{
#if defined(CONFIG_VSMP)
	    LOCK_ASSERT(cpulock.get_owner_tid() == L4_Myself(), '5', name());
	    LOCK_ASSERT(cpulock.get_owner_pcpu_id() == (word_t) L4_ProcessorNo(), '6', name());
	    //LOCK_DEBUG('u', L4_ProcessorNo, L4_Myself(), CONFIG_NR_VCPUS, L4_nilthread);
	    release();
#endif /* CONFIG_VSMP */
	}
    

};


#endif /* !__L4KA__SYNC_H__ */
