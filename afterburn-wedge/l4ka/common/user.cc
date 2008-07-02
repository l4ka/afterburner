/*********************************************************************
 *                
 * Copyright (C) 2008,  Karlsruhe University
 *                
 * File path:     user.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include <bind.h>
#include <debug.h>
#include <l4/kip.h>

#include INC_ARCH(bitops.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(l4thread.h)
#include INC_WEDGE(backend.h)

thread_manager_t thread_manager;
task_manager_t task_manager;

task_info_t *
task_manager_t::find_by_page_dir( L4_Word_t page_dir, bool alloc )
{
    task_info_t *ret = 0;
    L4_Word_t idx_start = hash_page_dir( page_dir );

    L4_Word_t idx = idx_start;
    do {
	if( tasks[idx].page_dir == page_dir )
	{
	    ret =  &tasks[idx];
	    goto done;
	}
	idx = (idx + 1) % max_tasks;
    } while( idx != idx_start );

    // TODO: go to an external process for dynamic memory.
 done:
    
    if (!ret && alloc)
    {
	ret = allocate( page_dir );
	ret->init();
    }

    return ret;
}

task_info_t *
task_manager_t::allocate( L4_Word_t page_dir )
{
    task_info_t *ret = NULL;
    
    L4_Word_t idx_start = hash_page_dir( page_dir );

    L4_Word_t idx = idx_start;
    do {
	if( tasks[idx].page_dir == 0 ) {
	    tasks[idx].page_dir = page_dir;
	    ret =  &tasks[idx];
	    goto done;
	}
	idx = (idx + 1) % max_tasks;
    } while( idx != idx_start );


 done:
    // TODO: go to an external process for dynamic memory.
    return ret;
}

void task_manager_t::deallocate( task_info_t *ti )
{ 
    //ptab_info.clear(ti->page_dir);
    ti->init();
    ti->page_dir = 0;

}



thread_info_t *
thread_manager_t::find_by_tid( L4_ThreadId_t tid )
{
    L4_Word_t start_idx = hash_tid( tid );

    L4_Word_t idx = start_idx;
    do {
	if( threads[idx].tid == tid )
	    return &threads[idx];
	idx = (idx + 1) % max_threads;
    } while( idx != start_idx );

    // TODO: go to an external process for dynamic memory.
    return 0;
}

thread_info_t *
thread_manager_t::allocate( L4_ThreadId_t tid )
{
    thread_info_t *ret = NULL;
    
    L4_Word_t start_idx = hash_tid( tid );
    L4_Word_t idx = start_idx;
    do {
	if( L4_IsNilThread(threads[idx].tid) ) {
	    threads[idx].tid = tid;
	    ret = &threads[idx];
	    goto done;
	}
	idx = (idx + 1) % max_threads;
    } while( idx != start_idx );

 done: 
    // TODO: go to an external process for dynamic memory.
    return ret;
}

void
thread_manager_t::dump()
{
    for (unsigned int i = 0; i < max_threads; i++)
    {
	if (threads[i].tid != L4_nilthread)
	{
	    printf( "thread: %t\n", threads[i].tid);
	    threads[i].mrs.dump(debug_id_t(0, 0));
	}
    }
}
