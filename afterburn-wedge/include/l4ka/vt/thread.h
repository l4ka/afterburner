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
#ifndef __L4KA__VM__THREAD_H__
#define __L4KA__VM__THREAD_H__

#include <l4/ipc.h>
#include INC_ARCH(cpu.h)
#include INC_ARCH(page.h)
#include INC_ARCH(types.h)
#include INC_WEDGE(debug.h)

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
    thread_state_running,
    thread_state_waiting_for_interrupt,
};


class mr_save_t
{
private:
    union {
	L4_Word_t raw[13];
	struct {
	    L4_MsgTag_t tag;
	} envelope;
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
    };
    

public:
    void init() 
    { envelope.tag.raw = 0; }
    
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

    void store_mrs(L4_MsgTag_t tag) 
	{
	    ASSERT (L4_UntypedWords(tag) + L4_TypedWords(tag) < 13);
	    L4_StoreMRs( 0, 
		    1 + L4_UntypedWords(tag) + L4_TypedWords(tag),
		    raw );
	}
    void load() 
	{
	    ASSERT (L4_UntypedWords(envelope.tag) + 
		    L4_TypedWords(envelope.tag) < 13);
	    L4_LoadMRs( 0, 
		    1 + L4_UntypedWords(envelope.tag) 
		    + L4_TypedWords(envelope.tag),
		    raw );
	}
    
    L4_MsgTag_t get_msg_tag() { return envelope.tag; }
    void set_msg_tag(L4_MsgTag_t t) { envelope.tag = t; }

    
    void load_pfault_reply(L4_MapItem_t map_item, iret_handler_frame_t *iret_emul_frame=NULL) 
	{
	    pfault_msg.tag.raw = 0;
	    pfault_msg.tag.X.u = 0;
	    pfault_msg.tag.X.t = 2;
	    pfault_msg.item = map_item;
	    load();
	}

    L4_Word_t get_pfault_ip() { return pfault_msg.ip; }
    L4_Word_t get_pfault_addr() { return pfault_msg.addr; }
    L4_Word_t get_pfault_rwx() { return L4_Label(pfault_msg.tag) & 0x7; }
    
    void load_startup_reply(word_t start_ip, word_t start_sp, bool rm);


    void dump();

};



#endif /* !__L4KA__VM__THREAD_H__ */
