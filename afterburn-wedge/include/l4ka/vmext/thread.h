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

#ifndef __L4KA__VMEXT__THREAD_H__
#define __L4KA__VMEXT__THREAD_H__
#include <l4/ia32/arch.h>
#include <l4/ipc.h>
#include INC_ARCH(cpu.h)
#include INC_ARCH(page.h)
#include INC_ARCH(types.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(message.h)


#define OFS_MR_SAVE_TAG		 0
#define OFS_MR_SAVE_PF_ADDR	 1
#define OFS_MR_SAVE_EXC_NO	 1
#define OFS_MR_SAVE_PF_IP	 1
#define OFS_MR_SAVE_ERRCODE	 2

#define OFS_MR_SAVE_EIP		(L4_CTRLXFER_GPREGS_EIP    + 4)
#define OFS_MR_SAVE_EFLAGS	(L4_CTRLXFER_GPREGS_EFLAGS + 4) 
#define OFS_MR_SAVE_EDI		(L4_CTRLXFER_GPREGS_EDI    + 4) 
#define OFS_MR_SAVE_ESI		(L4_CTRLXFER_GPREGS_ESI    + 4)  
#define OFS_MR_SAVE_EBP		(L4_CTRLXFER_GPREGS_EBP    + 4) 
#define OFS_MR_SAVE_ESP		(L4_CTRLXFER_GPREGS_ESP    + 4) 
#define OFS_MR_SAVE_EBX		(L4_CTRLXFER_GPREGS_EBX    + 4) 
#define OFS_MR_SAVE_EDX		(L4_CTRLXFER_GPREGS_EDX    + 4) 
#define OFS_MR_SAVE_ECX		(L4_CTRLXFER_GPREGS_ECX    + 4) 
#define OFS_MR_SAVE_EAX		(L4_CTRLXFER_GPREGS_EAX    + 4) 

enum thread_state_t { 
    thread_state_startup,
    thread_state_pfault,  
    thread_state_exception,
    thread_state_preemption,
};

class mr_save_t
{
    static const word_t fpu_state_size = ia32_fpu_t::fpu_state_words;;
private:
    union 
    {
	L4_Word_t raw[L4_CTRLXFER_GPREGS_ITEM_SIZE+3];
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
		L4_Word_t untyped[2];
	    };
	    L4_GPRegsCtrlXferItem_t gpregs_item;
	    
	    bool fpu_state_valid;
	    L4_Word_t fpu_state[fpu_state_size];
	};
    };
public:

    void init(){ tag.raw = 0; fpu_state_valid = false; }
    
    L4_Word_t get(word_t idx)
	{
	    ASSERT(idx < (L4_CTRLXFER_GPREGS_ITEM_SIZE+3));
	    return raw[idx];
	}
    void set(word_t idx, word_t val)
	{
	    ASSERT(idx < (L4_CTRLXFER_GPREGS_ITEM_SIZE+3));
	    raw[idx] = val;
	}

    void set_gpregs_item() 
	{ 
	    gpregs_item.item = L4_CtrlXferItem(L4_CTRLXFER_GPREGS_ID); 
	    gpregs_item.item.num_regs = L4_CTRLXFER_GPREGS_SIZE;
	    gpregs_item.item.mask = 0x3ff;
	}

    void store_mrs(L4_MsgTag_t t) 
	{	
	    if (L4_UntypedWords(t) != 2)
		DEBUGGER_ENTER("ASSERT STORE_MRS");
	    ASSERT (L4_UntypedWords(t) == 2);
	    ASSERT (L4_TypedWords(t) == L4_CTRLXFER_GPREGS_ITEM_SIZE);
	    L4_StoreMR( 0, &raw[0] );
	    L4_StoreMR( 1, &raw[1] );
	    L4_StoreMR( 2, &raw[2] );
	    L4_StoreMRs( 3, L4_CTRLXFER_GPREGS_ITEM_SIZE, gpregs_item.raw );
	    //L4_StoreGPRegsCtrlXferItem(3, &gpregs_item);
	}
    void load(word_t additional_untyped=0) 
	{	
	    tag.X.u += additional_untyped;
	    word_t mapitems = is_pfault_msg() ? 2 : 0;
	    
	    /* GP CtrlXfer Item */
	    L4_LoadMRs( 1 + L4_UntypedWords(tag) + mapitems, L4_TypedWords(tag)-mapitems, gpregs_item.raw );
	    /* Map Item */
	    L4_LoadMRs( 1 + L4_UntypedWords(tag), mapitems, pfault.item.raw );
	    /* Builtin untyped words (max 2) */
	    L4_LoadMRs( 1 + additional_untyped, L4_UntypedWords(tag) - additional_untyped, untyped);	
	    /* Tag */
	    L4_LoadMR ( 0, tag.raw);
	    clear_msg_tag();
	    
	}


    L4_MsgTag_t get_msg_tag() { return tag; }
    void set_msg_tag(L4_MsgTag_t t) { tag = t; }
    void clear_msg_tag() { tag.raw = 0; }
   
    void set_propagated_reply(L4_ThreadId_t virtualsender) 
	{ 
	    L4_Set_Propagation(&tag); 
	    L4_Set_VirtualSender(virtualsender);
	}

    bool is_exception_msg() { return L4_Label(tag) == msg_label_exception; }
    bool is_preemption_msg() 
	{ 
	    return (L4_Label(tag) >= msg_label_preemption &&
		    L4_Label(tag) <= msg_label_preemption_yield);
	}
    
    bool is_pfault_msg() 
	{ 
	    return (L4_Label(tag) >= msg_label_pfault_start &&
		    L4_Label(tag) <= msg_label_pfault_end);
	}
    
    L4_Word_t get_pfault_ip() { return gpregs_item.gprs.eip; }
    L4_Word_t get_pfault_addr() { return pfault.addr; }
    L4_Word_t get_pfault_rwx() { return L4_Label(tag) & 0x7; }

    L4_Word_t get_exc_ip() { return gpregs_item.gprs.eip; }
    void set_exc_ip(word_t ip) { gpregs_item.gprs.eip = ip; }
    L4_Word_t get_exc_sp() { return gpregs_item.gprs.esp; }
    L4_Word_t get_exc_number() { return exception.excno; }

    L4_Word64_t get_preempt_time() 
	{ return ((L4_Word64_t) preempt.time2 << 32) | ((L4_Word64_t) preempt.time1); }
    L4_Word_t get_preempt_ip() 
	{ return gpregs_item.gprs.eip; }
    L4_ThreadId_t get_preempt_target() 
	{ return (L4_ThreadId_t) { raw : gpregs_item.gprs.eax }; }
   

    static const L4_MsgTag_t pfault_reply_tag()
	{ return (L4_MsgTag_t) { X: { 0, 2 + L4_CTRLXFER_GPREGS_ITEM_SIZE, 0, msg_label_pfault_start} } ;}


    void load_pfault_reply(L4_MapItem_t map_item, iret_handler_frame_t *iret_emul_frame=NULL) 
	{
	    ASSERT(is_pfault_msg());
	    tag = pfault_reply_tag();
	    pfault.item = map_item;
	    
	    if (iret_emul_frame)
	    {
		for( u32_t i = 0; i < 9; i++ )
		    gpregs_item.gprs.reg[i+1] = iret_emul_frame->frame.x.raw[8-i];	
		gpregs_item.gprs.eflags = iret_emul_frame->iret.flags.x.raw;
		gpregs_item.gprs.eip = iret_emul_frame->iret.ip;
		gpregs_item.gprs.esp = iret_emul_frame->iret.sp;
	    }
	    set_gpregs_item();
	}
    
    static const L4_MsgTag_t exc_reply_tag()
	{ return (L4_MsgTag_t) { X: { 0, L4_CTRLXFER_GPREGS_ITEM_SIZE, 0, msg_label_exception} } ;}
    

    void load_exception_reply(bool enable_fpu, iret_handler_frame_t *iret_emul_frame) 
	{
	    ASSERT(is_exception_msg());
	    tag = exc_reply_tag();
	    
	    if (iret_emul_frame)
	    {
		for( u32_t i = 0; i < 9; i++ )
		    gpregs_item.gprs.reg[i+1] = iret_emul_frame->frame.x.raw[8-i];	
		gpregs_item.gprs.eflags = iret_emul_frame->iret.flags.x.raw;
		gpregs_item.gprs.eip = iret_emul_frame->iret.ip;
		gpregs_item.gprs.esp = iret_emul_frame->iret.sp;
	    }
	    set_gpregs_item();
	}
    
    
    static const L4_MsgTag_t startup_reply_tag()
	{ return (L4_MsgTag_t) { X: { 0, L4_CTRLXFER_GPREGS_ITEM_SIZE, 0, msg_label_startup_reply} } ;}

    void load_startup_reply(iret_handler_frame_t *iret_emul_frame) 
	{ 
	    for( u32_t i = 0; i < 9; i++ )
		gpregs_item.gprs.reg[i+1] = iret_emul_frame->frame.x.raw[8-i];
	    gpregs_item.gprs.eflags = iret_emul_frame->iret.flags.x.raw;
	    gpregs_item.gprs.eip = iret_emul_frame->iret.ip;
	    gpregs_item.gprs.esp = iret_emul_frame->iret.sp;
	    set_gpregs_item();
	    tag = startup_reply_tag();
	}

    void load_startup_reply(L4_Word_t ip, L4_Word_t sp) 
	{ 
	    gpregs_item.gprs.eip = ip;
	    gpregs_item.gprs.esp = sp;
	    set_gpregs_item();
 	    tag = startup_reply_tag();
	}
	    

    static const L4_MsgTag_t preemption_reply_tag(const bool cxfer)
	{ return (L4_MsgTag_t) { X: { 0, (cxfer ? L4_CTRLXFER_GPREGS_ITEM_SIZE : 0), 0, msg_label_preemption_reply} }; }


    void load_preemption_reply(bool cxfer, iret_handler_frame_t *iret_emul_frame=NULL) 
	{ 
	    ASSERT(is_preemption_msg());
	    
	    if (iret_emul_frame)
	    {
		for( u32_t i = 0; i < 9; i++ )
		    gpregs_item.gprs.reg[i+1] = iret_emul_frame->frame.x.raw[8-i];
		gpregs_item.gprs.eflags = iret_emul_frame->iret.flags.x.raw;
		gpregs_item.gprs.eip = iret_emul_frame->iret.ip;
		gpregs_item.gprs.esp = iret_emul_frame->iret.sp;
	    }
	    set_gpregs_item();
	    tag = preemption_reply_tag(cxfer);
	}

    static L4_MsgTag_t yield_tag(bool cxfer=true)
	{ return (L4_MsgTag_t) { X: { 2, (cxfer ? L4_CTRLXFER_GPREGS_ITEM_SIZE : 0), 0, msg_label_preemption_yield} } ;}

    void load_yield_msg(L4_ThreadId_t dest, bool cxfer=true) 
	{ 
	    tag = yield_tag(cxfer);
	    if (cxfer)
	    {
		gpregs_item.gprs.eax = dest.raw;
		set_gpregs_item();
	    }
	    L4_Accept(L4_UntypedWordsAcceptor);
	}

    void dump();

};



#endif /* !__L4KA__VMEXT__THREAD_H__ */
