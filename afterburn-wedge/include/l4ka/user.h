/*********************************************************************
 *                
 * Copyright (C) 2008,  Karlsruhe University
 *                
 * File path:     l4ka/user.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __L4KA__THREAD_INFO_H__
#define __L4KA__THREAD_INFO_H__

class task_manager_t;
class thread_manager_t;
class vcpu_t;

#if defined(CONFIG_L4KA_VM)
#include INC_WEDGE(mr_save.h)
#include INC_WEDGE(task.h)
#elif defined(CONFIG_L4KA_VMEXT)
#include INC_WEDGE(mr_save_cxfer.h)
#include INC_WEDGE(task_vmext.h)
#elif defined(CONFIG_L4KA_HVM)
#include INC_WEDGE(mr_save_cxfer.h)
#include INC_WEDGE(task_hvm.h)
#endif

class task_manager_t
{
    static const L4_Word_t max_tasks = 1024;
    task_info_t tasks[max_tasks];

    L4_Word_t hash_page_dir( L4_Word_t page_dir )
	{ return (page_dir >> PAGEDIR_BITS) % max_tasks; }

public:
    task_info_t * find_by_page_dir( L4_Word_t page_dir, bool alloc = false );
    task_info_t * allocate( L4_Word_t page_dir );
    void deallocate( task_info_t *ti );

    task_manager_t() { }
    
};


INLINE task_manager_t & get_task_manager()
{
    extern task_manager_t task_manager;
    return task_manager;
}

class thread_info_t
{
    L4_ThreadId_t tid;
    friend class thread_manager_t;

public:
    task_info_t *ti;
    thread_state_t state;
    mr_save_t mr_save;

    thread_info_t()
	{ tid = L4_nilthread; }
    
    void set_tid(L4_ThreadId_t t)
	{ tid = t; }
    L4_ThreadId_t get_tid()
	{ 
	    if (tid.raw == 0x4100000) DEBUGGER_ENTER("BUG"); return tid; 
	}

    bool is_space_thread()
	{ return L4_Version(tid) == 1; }

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
	{ ti->tid = L4_nilthread; ti->ti = NULL; }

    void dump();
    bool resume_vm_threads();
    
};


INLINE thread_manager_t & get_thread_manager()
{
    extern thread_manager_t thread_manager;
    return thread_manager;
}

bool handle_user_pagefault( vcpu_t &vcpu, thread_info_t *thread_info, L4_ThreadId_t tid, L4_MapItem_t &map_item);

#endif /* !__L4KA__THREAD_INFO_H__ */
