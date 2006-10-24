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
#ifndef __L4KA__USER_EXT_H__
#define __L4KA__USER_EXT_H__

#include <l4/ia32/arch.h>
#include <l4/ipc.h>
#include INC_ARCH(cpu.h)
#include INC_ARCH(page.h)
#include INC_ARCH(types.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(message.h)

#define OFS_MR_SAVE_TAG		 0
#define OFS_MR_SAVE_EXC_NO	 1
#define OFS_MR_SAVE_ERRCODE	 2
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

enum thread_state_t { 
    thread_state_pfault, 
    thread_state_exception,
    thread_state_preemption
};
class mr_save_t
{
private:
    union 
    {
	L4_Word_t raw[CTRLXFER_SIZE+3];
	struct {
	    L4_MsgTag_t tag;
	    union 
	    {
		union {
		    struct {
			L4_Word_t addr;
			L4_Word_t ip;
		    };
		    L4_MapItem_t item;
		} pfault;
		struct {
		    L4_Word_t excno;
		    L4_Word_t errcode;
		} exception;
		struct {
		    L4_Word_t time1;
		    L4_Word_t time2;
		} preempt;

	    };
	    L4_CtrlXferItem_t ctrlxfer;
	};
    };
public:
    void init(){ tag.raw = 0; }
    
    L4_Word_t get(word_t idx)
	{
	    ASSERT(idx < (CTRLXFER_SIZE+3));
	    return raw[idx];
	}
    void set(word_t idx, word_t val)
	{
	    ASSERT(idx < (CTRLXFER_REG_SIZE+3));
	    raw[idx] = val;
	}

    void store_mrs(L4_MsgTag_t t) 
	{	
	    ASSERT (L4_UntypedWords(t) == 2);
	    ASSERT (L4_TypedWords(t) == CTRLXFER_SIZE);
	    L4_StoreMR( 0, &raw[0] );
	    L4_StoreMR( 1, &raw[1] );
	    L4_StoreMR( 2, &raw[2] );
	    L4_StoreCtrlXferItem(3, &ctrlxfer);
	}
    void load_mrs(word_t idx=1) 
	{
	    ASSERT (L4_UntypedWords(tag) + L4_TypedWords(tag) 
		    <= (CTRLXFER_SIZE+3-idx));
	    L4_LoadMR(  0 , tag.raw);
	    L4_LoadMRs( 1, 
		    L4_UntypedWords(tag) + L4_TypedWords(tag),
		    &raw[idx] );
	}

    L4_MsgTag_t get_msg_tag() { return tag; }
    void set_msg_tag(L4_MsgTag_t t) { tag = t; }
    void clear_msg_tag() { tag.raw = 0; }
    
    void set_propagated_reply(L4_ThreadId_t virtualsender) 
	{ 
	    L4_Set_Propagation(&tag); 
	    L4_Set_VirtualSender(virtualsender);
	}

    bool is_preemption_msg() { return L4_Label(tag) == msg_label_preemption; }
    
    L4_Word_t get_pfault_ip() { return ctrlxfer.eip; }
    L4_Word_t get_pfault_addr() { return pfault.addr; }
    L4_Word_t get_pfault_rwx() { return L4_Label(tag) & 0x7; }

    L4_Word_t get_exc_ip() { return ctrlxfer.eip; }
    void set_exc_ip(word_t ip) { ctrlxfer.eip = ip; }
    L4_Word_t get_exc_sp() { return ctrlxfer.esp; }

    L4_Word64_t get_preempt_time() 
	{ return ((L4_Word64_t) preempt.time2 << 32) | ((L4_Word64_t) preempt.time1); }


    
    void load_pfault_reply(L4_MapItem_t map_item) 
	{
	    tag.X.u = 0;
	    tag.X.t = 2 + CTRLXFER_SIZE;
	    pfault.item = map_item;
	    L4_InitCtrlXferItem(&ctrlxfer, 0x3ff);
	    load_mrs();
	    clear_msg_tag();
	}

    void load_exception_reply(iret_handler_frame_t *iret_emul_frame) 
	{
	    for( u32_t i = 0; i < 9; i++ )
		ctrlxfer.regs[i+1] = iret_emul_frame->frame.x.raw[8-i];
	    ctrlxfer.eip = iret_emul_frame->iret.ip;
	    ctrlxfer.esp = iret_emul_frame->iret.sp;
	    
	    tag.X.u = 0;
	    tag.X.t = CTRLXFER_SIZE;
	    L4_InitCtrlXferItem(&ctrlxfer, 0x3ff);
	    load_mrs(3);
	    clear_msg_tag();

	}
    void load_startup_reply(iret_handler_frame_t *iret_emul_frame) 
	{ load_exception_reply(iret_emul_frame); }
    void load_preemption_reply() 
	{ 
	    tag.X.u = 0;
	    tag.X.t = CTRLXFER_SIZE;
	    L4_InitCtrlXferItem(&ctrlxfer, 0x3ff);
	    load_mrs(3);
	    clear_msg_tag();

	}

};


#endif /* !__L4KA__USER_EXT_H__ */
