/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/l4-common/user.h
 * Description:   Data types for mapping L4 threads to guest OS threads.
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
 * $Id: user.h,v 1.9 2005/09/05 14:10:05 joshua Exp $
 *
 ********************************************************************/

#ifndef __L4KA__VM_H__
#define __L4KA__VM_H__

#include INC_ARCH(page.h)
#include INC_ARCH(types.h)

#if defined(CONFIG_L4KA_VMEXTENSIONS)
#include INC_WEDGE(thread_ext.h)
#else
#include INC_WEDGE(thread.h)
#endif


// TODO: protect with locks to make SMP safe.
#if defined(CONFIG_SMP)
#error Not SMP safe!!
#endif
static const bool debug_helper=0;

extern word_t user_vaddr_end;

class task_manager_t;
class thread_manager_t;
class thread_info_t;

class task_info_t
{
    // Note: we use the index of the utcb_mask as the TID version (plus 1).
    // The space_tid is the first thread, and thus the first utcb.
    // The space thread is the last to be deleted.

    static const L4_Word_t max_threads = CONFIG_L4_MAX_THREADS_PER_TASK;
    static L4_Word_t utcb_size, utcb_base;

    L4_Word_t page_dir;
    word_t utcb_mask[ max_threads/sizeof(L4_Word_t) + 1 ];
    L4_ThreadId_t space_tid;
#if defined(CONFIG_VSMP)
    thread_info_t *vcpu_thread[CONFIG_NR_VCPUS];
#endif
    
    friend class task_manager_t;

public:
    L4_Fpage_t utcb_fp;
    L4_Fpage_t kip_fp;
    
    task_info_t();
    void init();

    static word_t encode_gtid_version( word_t value )
	// Ensure that a bit is always set in the lower six bits of the
	// global thread ID's version field.
	{ return (value << 1) | 1; }
    static word_t decode_gtid_version( L4_ThreadId_t gtid )
	{ return L4_Version(gtid) >> 1; }

    bool utcb_allocate( L4_Word_t & utcb, L4_Word_t & index );
    void utcb_release( L4_Word_t index );

    word_t thread_count();

    bool has_space_tid()
	{ return !L4_IsNilThread(space_tid); }
    L4_ThreadId_t get_space_tid()
	{ return L4_GlobalId( L4_ThreadNo(space_tid), encode_gtid_version(0)); }
    void set_space_tid( L4_ThreadId_t tid )
	{ space_tid = tid; }
    void invalidate_space_tid()
	{ space_tid = L4_GlobalId( L4_ThreadNo(space_tid), 0 ); }
    bool is_space_tid_valid()
	{ return 0 != L4_Version(space_tid); }
    L4_Word_t get_page_dir()
	{ return page_dir; }

#if defined(CONFIG_VSMP)
    thread_info_t *get_vcpu_thread(word_t vcpu_id)
	{ return vcpu_thread[vcpu_id]; } 
    void set_vcpu_thread(word_t vcpu_id, thread_info_t *thread)
	{ vcpu_thread[vcpu_id] = thread; } 
#endif

#if defined(CONFIG_L4KA_VMEXTENSIONS)
private:
    static const L4_Word_t unmap_cache_size = 64 - 2 * CTRLXFER_SIZE;
    L4_Fpage_t unmap_pages[unmap_cache_size];
    L4_Word_t unmap_count;
public:
    bool has_unmap_pages() { return unmap_count != 0; }
    bool add_unmap_page(L4_Fpage_t fpage)
	{
	    if( unmap_count == unmap_cache_size )
		return false;
	    
	    unmap_pages[unmap_count++] = fpage;
	    return true;
	}
    L4_Word_t task_info_t::commit_helper(bool piggybacked);
#endif

};

class task_manager_t
{
    static const L4_Word_t max_tasks = 1024;
    task_info_t tasks[max_tasks];

    L4_Word_t hash_page_dir( L4_Word_t page_dir )
	{ return (page_dir >> PAGEDIR_BITS) % max_tasks; }

public:
    task_info_t * find_by_page_dir( L4_Word_t page_dir );
    task_info_t * allocate( L4_Word_t page_dir );
    void deallocate( task_info_t *ti );

    task_manager_t();

    static task_manager_t & get_task_manager()
	{
	    extern task_manager_t task_manager;
	    return task_manager;
	}
};

class thread_info_t
{
    L4_ThreadId_t tid;
    friend class thread_manager_t;

public:
    word_t vcpu_id;
    task_info_t *ti;

    thread_state_t state;
    mr_save_t mr_save;
	
public:
    L4_ThreadId_t get_tid()
	{ return tid; }
    
    bool is_space_thread()
	{ return L4_Version(tid) == 1; }

    thread_info_t();
    void init()
	{ ti = 0; mr_save.init(); }
};

class thread_manager_t
{
    static const L4_Word_t max_threads = 2048;
    thread_info_t threads[max_threads];

    L4_Word_t hash_tid( L4_ThreadId_t tid )
	{ return L4_ThreadNo(tid) % max_threads; }

public:
    thread_info_t * find_by_tid( L4_ThreadId_t tid );
    thread_info_t * allocate( L4_ThreadId_t tid );
    void deallocate( thread_info_t *ti )
	{ ti->tid = L4_nilthread; }

    thread_manager_t();

    static thread_manager_t & get_thread_manager()
	{
	    extern thread_manager_t thread_manager;
	    return thread_manager;
	}
};

class vcpu_t;
bool handle_user_pagefault( vcpu_t &vcpu, thread_info_t *thread_info, L4_ThreadId_t tid );

#endif /* !__L4KA__VM_H__ */
