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

#ifndef __L4KA__X2__MR_SAVE_H__
#define __L4KA__X2__MR_SAVE_H__

#include <debug.h>
#include <l4/ipc.h>
#include INC_ARCH(cpu.h)
#include INC_ARCH(page.h)
#include INC_ARCH(types.h)

// TODO: protect with locks to make SMP safe.
#if defined(CONFIG_SMP)
#error Not SMP safe!!
#endif

#define OFS_MR_SAVE_TAG		 0
#define OFS_MR_SAVE_EIP		 1 
#define OFS_MR_SAVE_EFLAGS	 2 
#define OFS_MR_SAVE_EXC_NO	 3
#define OFS_MR_SAVE_ERRCODE	 4
#define OFS_MR_SAVE_EDI		 5
#define OFS_MR_SAVE_ESI		 6  
#define OFS_MR_SAVE_EBP		 7 
#define OFS_MR_SAVE_ESP		 8 
#define OFS_MR_SAVE_EBX		 9 
#define OFS_MR_SAVE_EDX	 	10
#define OFS_MR_SAVE_ECX		11
#define OFS_MR_SAVE_EAX		12

enum thread_state_t { 
    thread_state_user, 
    thread_state_force, 
    thread_state_pending, 
    thread_state_except_reply 
};

class mrs_t
{
private:
    union {
	L4_Word_t raw[13];
	L4_MsgTag_t tag;
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
	    union {
		struct {
		    L4_Word_t addr;
		    L4_Word_t ip;
		};
		L4_MapItem_t item;
	    };
	} pfault_msg;
	struct {
	    L4_MsgTag_t tag;
	    L4_Word_t ip;
	    L4_Word_t sp;
	} startup_msg;
	struct {
	    L4_MsgTag_t tag;
	    L4_Word_t vector;
	} vector_msg;

    };
public:
    void init() { tag.raw = 0; }

        
    L4_Word_t get(word_t idx)
	{
	    ASSERT(idx < 13);
	    return raw[idx];
	}
    void set(word_t idx, word_t val)
	{
	    ASSERT(idx < 13);
	    raw[idx] = val;
	}

    void store() 
	{
	    
	    tag = L4_MsgTag();
	    ASSERT (L4_UntypedWords(tag) + L4_TypedWords(tag) < 13);
	    L4_StoreMRs( 1, L4_UntypedWords(tag) + L4_TypedWords(tag), raw + 1 );
	}
    
    void load() 
	{
	    ASSERT (L4_UntypedWords(tag) + L4_TypedWords(tag) < 13);
	    L4_LoadMRs( 0, 1 + L4_UntypedWords(tag) + L4_TypedWords(tag), raw );
	}
    
    void load_pfault_reply(L4_MapItem_t map_item, iret_handler_frame_t *iret_emul_frame=NULL) 
	{
	    
	    pfault_msg.tag.raw = 0;
	    pfault_msg.tag.X.u = 0;
	    pfault_msg.tag.X.t = 2;
	    pfault_msg.item = map_item;
	    load();
	}
    
    void load_startup_reply(word_t start_ip, word_t start_sp, iret_handler_frame_t *iret_emul_frame=NULL) 
	{
	    if (iret_emul_frame)
	    {
		for( u32_t i = 0; i < 8; i++ )
		    raw[5+7-i] = iret_emul_frame->frame.x.raw[i];
		
		exc_msg.eflags = iret_emul_frame->iret.flags.x.raw;
		exc_msg.eip = iret_emul_frame->iret.ip;
		exc_msg.esp = iret_emul_frame->iret.sp;
		
		L4_MsgTag_t start_tag = { raw : 0 };
		start_tag.X.u = 2;
		start_tag.X.t = 0;
		L4_Set_MsgTag(start_tag);
		L4_LoadMR(1, start_ip);
		L4_LoadMR(2, start_sp);

	    }
	    else
	    {
		startup_msg.ip = start_ip;
		startup_msg.sp = start_sp;
		startup_msg.tag.X.u = 2;
		startup_msg.tag.X.t = 0;
		load();
	    }

	}
    void load_exception_reply(bool unused, iret_handler_frame_t *iret_emul_frame) 
	{
	    for( u32_t i = 0; i < 8; i++ )
		raw[5+7-i] = iret_emul_frame->frame.x.raw[i];
	    exc_msg.eflags = iret_emul_frame->iret.flags.x.raw;
	    exc_msg.eip = iret_emul_frame->iret.ip;
	    exc_msg.esp = iret_emul_frame->iret.sp;
	
	    // Load the message registers.
	    pfault_msg.tag.raw = 0;
	    pfault_msg.tag.X.u = 12;
	    load();
	    L4_LoadMRs( 0, 1 + L4_UntypedWords(tag), raw );
	
	}

    L4_MsgTag_t get_msg_tag() { return tag; }
    void set_msg_tag(L4_MsgTag_t t) { tag = t; }

    L4_Word_t get_pfault_ip() { return pfault_msg.ip; }
    L4_Word_t get_pfault_addr() { return pfault_msg.addr; }
    L4_Word_t get_pfault_rwx() { return L4_Label(pfault_msg.tag) & 0x7; }

    L4_Word_t get_exc_ip() { return exc_msg.eip; }
    void set_exc_ip(word_t ip) { exc_msg.eip = ip; }
    L4_Word_t get_exc_sp() { return exc_msg.esp; }
    L4_Word_t get_exc_number() { return exc_msg.exc_no; }

    void set_propagated_reply(L4_ThreadId_t virtualsender) 
	{ 
	    L4_Set_Propagation(&tag); 
	    L4_Set_VirtualSender(virtualsender);
	}
    void dump(debug_id_t id, bool extended=false);
};



#endif /* !__L4KA__X2__MR_SAVE_H__ */
