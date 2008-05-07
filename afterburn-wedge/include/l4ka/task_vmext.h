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
#ifndef __L4KA__CXFER__THREAD_INFO_H__
#define __L4KA__CXFER__THREAD_INFO_H__

#include INC_ARCH(page.h)
#include INC_ARCH(types.h)
#include INC_WEDGE(sync.h)

class vcpu_t;
static const bool debug_helper=1;
extern cpu_lock_t thread_mgmt_lock;

extern word_t user_vaddr_end;

class task_info_t
{
    thread_info_t *vcpu_thread[CONFIG_NR_VCPUS];
    L4_Word_t page_dir;
    L4_Word_t vcpu_thread_count;
    L4_Word_t vcpu_ref_count;
    bool freed;

    L4_ThreadId_t space_tid;
    L4_Fpage_t utcb_fp;
    L4_Fpage_t kip_fp;
    friend class task_manager_t;

    static const L4_Word_t unmap_cache_size = 63 - (L4_CTRLXFER_GPREGS_SIZE + 1) ;
    L4_Word_t unmap_count;
    L4_Fpage_t unmap_pages[unmap_cache_size];

    static L4_Word_t utcb_size, utcb_area_size, utcb_area_base;
    
private:
    thread_info_t *allocate_vcpu_thread();
public:
    void free();

public:
    static word_t encode_gtid_version( word_t value )
    // Ensure that a bit is always set in the lower six bits of the
    // global thread ID's version field.
	{ return (value << 1) | 1; }
    static word_t decode_gtid_version( L4_ThreadId_t gtid )
	{ return L4_Version(gtid) >> 1; }
    
    
    void init();

    L4_Word_t get_page_dir()
	{ return page_dir; }

    void set_space_tid(L4_ThreadId_t s)
	{ space_tid = s; }

    thread_info_t *get_vcpu_thread(word_t vcpu_id, bool allocate=false)
    	{ 
	    if (!vcpu_thread[vcpu_id] && allocate)
		allocate_vcpu_thread();
	    return vcpu_thread[vcpu_id]; 
	
	}
    
    void inc_ref_count()
	{ atomic_inc(&vcpu_ref_count); }
    void dec_ref_count()
	{ 
	    if (atomic_dec_and_test(&vcpu_ref_count) &&	freed)
		free();
	}
    
    void schedule_free() { freed = true; }
    
    bool has_unmap_pages() { return unmap_count != 0; }
    void add_unmap_page(L4_Fpage_t fpage)
	{
#if defined(CONFIG_VSMP)
	    thread_mgmt_lock.lock();
#endif
	    if( unmap_count == unmap_cache_size )
		commit_helper();
	    ASSERT(unmap_count < unmap_cache_size);
	    unmap_pages[unmap_count++] = fpage;
#if defined(CONFIG_VSMP)
	    thread_mgmt_lock.unlock();
#endif
	}
    L4_Word_t commit_helper();

};

INLINE void setup_thread_faults(L4_ThreadId_t tid, bool on, bool unused) 
{
    /* Turn off ctrlxfer items */
    L4_Msg_t ctrlxfer_msg;
    L4_Word64_t fault_id_mask = (1<<2) | (1<<3) | (1<<5);
    L4_Word_t fault_mask = on ? L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_GPREGS_ID) : 0;	
    
    L4_Clear(&ctrlxfer_msg);
    L4_AppendFaultConfCtrlXferItems(&ctrlxfer_msg, fault_id_mask, fault_mask, false);
    L4_Load(&ctrlxfer_msg);
    L4_ConfCtrlXferItems(tid);

}

class l4thread_t;
typedef void (*l4thread_func_t)( void *, l4thread_t * );
class backend_vcpu_init_t;

#endif /* !__L4KA__CXFER__THREAD_INFO_H__ */
