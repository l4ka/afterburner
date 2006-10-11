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
#ifndef __L4KA__USER_H__
#define __L4KA__USER_H__

#include INC_ARCH(page.h)
#include INC_ARCH(types.h)
#include INC_WEDGE(vcpulocal.h)
#include <l4/ia32/arch.h>

// TODO: protect with locks to make SMP safe.
#if defined(CONFIG_SMP)
#error Not SMP safe!!
#endif

extern word_t user_vaddr_end;

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

#if !defined(CONFIG_L4KA_VMEXTENSIONS)
#define OFS_MR_SAVE_EIP		 1 
#define OFS_MR_SAVE_EFLAGS	 2 
#define OFS_MR_SAVE_EDI		 5
#define OFS_MR_SAVE_ESI		 6  
#define OFS_MR_SAVE_EBP		 7 
#define OFS_MR_SAVE_ESP		 8 
#define OFS_MR_SAVE_EBX		 9 
#define OFS_MR_SAVE_EDX	 	10
#define OFS_MR_SAVE_ECX		11
#define OFS_MR_SAVE_EAX		12

#define OFS_MR_SAVE_EXC_NO	 3
#define OFS_MR_SAVE_ERRCODE	 4

#else

#define OFS_MR_SAVE_EIP		 4
#define OFS_MR_SAVE_EFLAGS	 5 
#define OFS_MR_SAVE_EDI		 6 
#define OFS_MR_SAVE_ESI		 7  
#define OFS_MR_SAVE_EBP		 8 
#define OFS_MR_SAVE_ESP		 9 
#define OFS_MR_SAVE_EBX		10 
#define OFS_MR_SAVE_EDX		11 
#define OFS_MR_SAVE_ECX		12 
#define OFS_MR_SAVE_EAX		13 

#define OFS_MR_SAVE_EXC_NO	 1
#define OFS_MR_SAVE_ERRCODE	 2

#endif

class thread_info_t
{
    L4_ThreadId_t tid;

    friend class thread_manager_t;

public:
    task_info_t *ti;

    enum {
	state_user, state_force, state_pending, state_except_reply
    } state;
    
    // TODO: currently ia32 specific ... but so is our entire algorithm
    class mr_save_t
    {
    public:
	union {
	    L4_Word_t raw[13];
	    struct {
		L4_MsgTag_t tag;
	    } envelope;
	    struct {
		L4_MsgTag_t tag;
		L4_Word_t eip;
		L4_Word_t eflags;
		L4_Word_t exc_no;
		L4_Word_t error_code;
		L4_Word_t edi;
		L4_Word_t esi;
		L4_Word_t ebp;
		L4_Word_t esp;
		L4_Word_t ebx;
		L4_Word_t edx;
		L4_Word_t ecx;
		L4_Word_t eax;
	    } exc_msg;
	    struct {
		L4_MsgTag_t tag;
		L4_Word_t addr;
		L4_Word_t ip;
	    } pfault_msg;
	    struct {
		L4_MsgTag_t tag;
		L4_Word_t vector;
	    } vector_msg;
	};
    };
    
    class ext_mr_save_t
    {
    public:
	union 
	{
	    L4_Word_t raw[13];
	    struct {
		L4_MsgTag_t tag;
		union 
		{
		    struct {
			L4_Word_t addr;
			L4_Word_t ip;
		    } pfault;
		    struct {
			L4_Word_t excno;
			L4_Word_t errcode;
		    } exc;
		    struct {
			L4_Word_t time1;
			L4_Word_t time2;
		    } preempt;

		};
		L4_CtrlXferItem_t ctrlxfer;
	    };
	};
    };
    
    union {
	mr_save_t mr_save;
#if defined(CONFIG_L4KA_VMEXTENSIONS)
	ext_mr_save_t ext_mr_save;
#endif
    };
	
public:
#if defined(CONFIG_L4KA_VMEXTENSIONS)
    L4_Word_t get_pfault_ip() { return ext_mr_save.ctrlxfer.eip; }
    L4_Word_t get_pfault_addr() { return ext_mr_save.pfault.addr; }
    L4_Word_t get_pfault_rwx() { return L4_Label(ext_mr_save.tag) & 0x7; }
    void store_pfault_msg(L4_MsgTag_t tag) 
    {	
	ASSERT (L4_UntypedWords(tag) == 2);
	ASSERT (L4_TypedWords(tag) == CTRLXFER_SIZE);
	L4_StoreMR( 0, &ext_mr_save.tag.raw );
	L4_StoreMR( 1, &ext_mr_save.pfault.addr );
	L4_StoreMR( 2, &ext_mr_save.pfault.ip );
	L4_StoreCtrlXferItem(3, &ext_mr_save.ctrlxfer);
    }
    void load_pfault_msg(L4_MapItem_t map_item) 
    {
	L4_Msg_t msg;
	L4_MsgClear( &msg );
	L4_MsgAppendMapItem( &msg, map_item );
	L4_InitCtrlXferItem(&ext_mr_save.ctrlxfer, 0x3ff);
	L4_AppendCtrlXferItem(&msg, &ext_mr_save.ctrlxfer);
	L4_MsgLoad( &msg );
    }
    
#else /* defined(CONFIG_L4KA_VMEXTENSIONS) */
    L4_Word_t get_pfault_ip() { return mr_save.pfault.ip; }
    L4_Word_t get_pfault_addr() { return mr_save.pfault_msg.addr; }
    L4_Word_t get_pfault_rwx() { return L4_Label(mr_save.pfault_msg.tag) & 0x7; }
    L4_Word_t store_pfault_msg() 
    {
	ASSERT (L4_UntypedWords(tag) == 2);
	L4_StoreMR( 0, mr_save.pfault_msg.tag );
	L4_StoreMR( 1, mr_save.pfault_msg.addr );
	L4_StoreMR( 2, mr_save.pfault_msg.ip );
    }
    void load_pfault_msg(L4_MapItem_t map_item) 
    {
	L4_Msg_t msg;
	L4_MsgClear( &msg );
	L4_MsgAppendMapItem( &msg, map_item );
	L4_MsgLoad( &msg );
    }
    
#endif   
    
    L4_ThreadId_t get_tid()
	{ return tid; }
    
    bool is_space_thread()
	{ return L4_Version(tid) == 1; }

    thread_info_t();
    void init()
	{ ti = 0; mr_save.envelope.tag.raw = 0; }
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

bool handle_user_pagefault( vcpu_t &vcpu, thread_info_t *thread_info, L4_ThreadId_t tid );

#endif /* !__L4KA__USER_H__ */
