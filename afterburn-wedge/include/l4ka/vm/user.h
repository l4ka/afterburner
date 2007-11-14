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


#ifndef __L4KA__VM__USER_H__
#define __L4KA__VM__USER_H__
#include INC_ARCH(page.h)
#include INC_ARCH(types.h)
#include INC_WEDGE(vm/thread.h)

extern word_t user_vaddr_end;
class vcpu_t;
class task_manager_t;
class thread_manager_t;

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

    friend class task_manager_t;

public:
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

    bool has_one_thread();

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
    void deallocate( task_info_t *ti )
	{ ti->page_dir = 0; }

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
    task_info_t *ti;

    thread_state_t state;

    mr_save_t mr_save;

    void set_tid(L4_ThreadId_t t)
	{ tid = t; }
    L4_ThreadId_t get_tid()
	{ return tid; }

    bool is_space_thread()
	{ return L4_Version(tid) == 1; }

    thread_info_t();
    void init()
	{ ti = 0; mr_save.set_msg_tag((L4_MsgTag_t) {raw : 0 }); }
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


    bool resume_vm_threads() { UNIMPLEMENTED(); }
    thread_manager_t();

};


static thread_manager_t & get_thread_manager()
{
    extern thread_manager_t thread_manager;
    return thread_manager;
}

bool handle_user_pagefault( vcpu_t &vcpu, thread_info_t *thread_info, L4_ThreadId_t tid, L4_MapItem_t &map_item);
thread_info_t *allocate_thread();
void delete_thread( thread_info_t *thread_info );
#endif /* !__L4KA__VM__USER_H__ */
