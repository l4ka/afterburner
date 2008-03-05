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
#ifndef __L4KA__CXFER__MR_SAVE_H__
#define __L4KA__CXFER__MR_SAVE_H__

#include <debug.h>
#include <l4/ia32/arch.h>
#include <l4/ipc.h>
#include INC_ARCH(cpu.h)
#include INC_ARCH(page.h)
#include INC_ARCH(types.h)
#include INC_WEDGE(message.h)


#define OFS_MR_SAVE_TAG		 0
#define OFS_MR_SAVE_PF_ADDR	 1
#define OFS_MR_SAVE_EXC_NO	 1
#define OFS_MR_SAVE_PF_IP	 2
#define OFS_MR_SAVE_ERRCODE	 2

#define OFS_MR_SAVE_CTRLXFER				    (3)
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
    thread_state_activated,
};

class mr_save_t
{
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
	};
    };

   
public:
    
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

    void append_gpregs_item() 
	{ 
	    L4_CtrlXferItemInit(&gpregs_item.item, L4_CTRLXFER_GPREGS_ID); 
	    gpregs_item.item.num_regs = L4_CTRLXFER_GPREGS_SIZE;
	    gpregs_item.item.mask = 0x3ff;
	}

#if defined(CONFIG_L4KA_HVM)
    L4_Word_t next_eip, instr_len;
    /* CR0 .. CR4 */
    L4_RegCtrlXferItem_t cr_item[4];
    /* CS, SS, DS, ES, FS, GS, TR, LDTR, GDTR, IDTR */
    L4_SegmentCtrlXferItem_t seg_item[10];
    L4_MSRCtrlXferItem_t     msr_item;
    L4_ExcInjectCtrlXferItem_t exc_item;
#endif


    void init()
	{ 
	    tag.raw = 0;
	    L4_GPRegsCtrlXferItemInit(&gpregs_item); 
#if defined(CONFIG_L4KA_HVM)
	    for (word_t cr=0; cr<4; cr++)
		L4_RegCtrlXferItemInit(&cr_item[cr], L4_CTRLXFER_CREGS_ID);
	    for (word_t seg=0; seg<10; seg++)
		L4_SegmentCtrlXferItemInit(&seg_item[seg], L4_CTRLXFER_CSREGS_ID + seg);
	    L4_MSRCtrlXferItemInit(&msr_item, L4_CTRLXFER_MSR_ID); 
	    L4_ExcInjectCtrlXferItemInit(&exc_item); 
#endif
	}
    
    void store_mrs(L4_MsgTag_t t) 
	{	
	    ASSERT (t.X.u == 2);
	    ASSERT (t.X.t == L4_CTRLXFER_GPREGS_ITEM_SIZE);
	    
	    L4_StoreMR(  0, &raw[0] );
	    L4_StoreMR(  1, &raw[1] );
	    L4_StoreMR(  2, &raw[2] );
	    L4_StoreMRs( 3, L4_CTRLXFER_GPREGS_ITEM_SIZE, gpregs_item.raw );
	    /* Reset num_regs, since it's used as write indicator */
	    gpregs_item.item.num_regs = 0;
	    
	}
    
    void load(word_t additional_untyped=0) 
	{	
	    tag.X.u += additional_untyped;
	    
	    /* map item, if needed */
	    L4_LoadMRs( 1 + tag.X.u, tag.X.t, pfault.item.raw );
	    
	    /* GP CtrlXfer Item */
	    if (gpregs_item.item.num_regs)
	    {
		L4_LoadMRs(1 + tag.X.u + tag.X.t, 1 + gpregs_item.item.num_regs, gpregs_item.raw);
		tag.X.t += 1 + gpregs_item.item.num_regs;
		gpregs_item.item.num_regs = 0;
	    }
#if defined(CONFIG_L4KA_HVM)
	    /* CR Item */
	    for (word_t cr=0; cr<4; cr++)
		if (cr_item[cr].item.num_regs)
	    {
		L4_LoadMRs(1 + tag.X.u + tag.X.t, 1 + cr_item[cr].item.num_regs, cr_item[cr].raw);
		tag.X.t += 1 + cr_item[cr].item.num_regs;
		cr_item[cr].item.num_regs = 0;
	    }
	    /* Seg Item */
	    for (word_t seg=0; seg<10; seg++)
		if (seg_item[seg].item.num_regs)
	    {
		L4_LoadMRs(1 + tag.X.u + tag.X.t, 1 + seg_item[seg].item.num_regs, seg_item[seg].raw);
		tag.X.t += 1 + seg_item[seg].item.num_regs;
		seg_item[seg].item.num_regs = 0;
	    }
	    /* MSR CtrlXfer Item */
	    if (msr_item.item.num_regs)
	    {
		L4_LoadMRs(1 + tag.X.u + tag.X.t, 1 + msr_item.item.num_regs, msr_item.raw);
		tag.X.t += 1 + msr_item.item.num_regs;
		msr_item.item.num_regs = 0;
	    }
	    /* Exception CtrlXfer Item */
	    if (exc_item.item.num_regs)
	    {
		L4_LoadMRs(1 + tag.X.u + tag.X.t, 1 + exc_item.item.num_regs, exc_item.raw);
		tag.X.t += 1 + exc_item.item.num_regs;
		exc_item.item.num_regs = 0;
	    }
#endif
	    
	    /* Builtin untyped words (max 2) */
	    ASSERT(tag.X.u - additional_untyped <= 2);
	    L4_LoadMRs( 1 + additional_untyped, tag.X.u - additional_untyped, untyped);	
	    
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
   
    void load_iret_emul_frame(iret_handler_frame_t *frame)
	{
	    for( u32_t i = 0; i < 9; i++ )
		gpregs_item.gprs.reg[i+1] = frame->frame.x.raw[8-i];	
	    
	    gpregs_item.gprs.eflags = frame->iret.flags.x.raw;
	    gpregs_item.gprs.eip = frame->iret.ip;
	    gpregs_item.gprs.esp = frame->iret.sp;
	}
    

    static const L4_MsgTag_t pfault_reply_tag()
	{ return (L4_MsgTag_t) { X: { 0, 2, 0, msg_label_pfault_start} } ;}


    void load_pfault_reply(L4_MapItem_t map_item, iret_handler_frame_t *iret_emul_frame=NULL) 
	{
	    ASSERT(is_pfault_msg());
	    tag = pfault_reply_tag();
	    pfault.item = map_item;
	    
	    if (iret_emul_frame) 
		load_iret_emul_frame(iret_emul_frame);
		
	    append_gpregs_item();
	    dump(debug_pfault+1);
	}
    
    static const L4_MsgTag_t exc_reply_tag()
	{ return (L4_MsgTag_t) { X: { 0, 0, 0, msg_label_exception} } ;}
    

    void load_exception_reply(bool enable_fpu, iret_handler_frame_t *iret_emul_frame) 
	{
	    ASSERT(is_exception_msg());
	    tag = exc_reply_tag();
	    
	    if (iret_emul_frame) 
		load_iret_emul_frame(iret_emul_frame);
	    
	    append_gpregs_item();
	    dump(debug_exception+1);
	}
    
    
    static const L4_MsgTag_t startup_reply_tag()
	{ return (L4_MsgTag_t) { X: { 0, 0, 0, msg_label_startup_reply} } ;}

    void load_startup_reply(iret_handler_frame_t *iret_emul_frame) 
	{ 
	    load_iret_emul_frame(iret_emul_frame);
	    tag = startup_reply_tag();
	    append_gpregs_item();
	    dump(debug_task+1);
	}
    
    void load_startup_reply(word_t start_ip, word_t start_sp, word_t start_cs, word_t start_ss, bool rm);

	    
    static const L4_MsgTag_t preemption_reply_tag()
	{ return (L4_MsgTag_t) { X: { 0, 0, 0, msg_label_preemption_reply} }; }

    void load_preemption_reply(bool cxfer, iret_handler_frame_t *iret_emul_frame=NULL) 
	{ 
	    ASSERT(is_preemption_msg());
	    
	    if (iret_emul_frame)
		load_iret_emul_frame(iret_emul_frame);
	    
	    tag = preemption_reply_tag();
	    if (cxfer) append_gpregs_item();
	    dump(debug_preemption+1);
	}

    static L4_MsgTag_t yield_tag()
	{ return (L4_MsgTag_t) { X: { 2, 0, 0, msg_label_preemption_yield} } ;}

    void load_yield_msg(L4_ThreadId_t dest, bool cxfer=true) 
	{ 
	    tag = yield_tag();
	    if (cxfer)
	    {
		gpregs_item.gprs.eax = dest.raw;
		append_gpregs_item();
	    }
	    else
		L4_LoadMR(1, dest.raw);
	    L4_Accept(L4_UntypedWordsAcceptor);
	}

    void load_activation_reply(iret_handler_frame_t *iret_emul_frame=NULL) 
	{ 
	    if (iret_emul_frame)
		load_iret_emul_frame(iret_emul_frame);
    
	    dump(debug_preemption+1);
	}

    void load_startup_reply(L4_Word_t ip, L4_Word_t sp) 
	{ 
	    gpregs_item.gprs.eip = ip;
	    gpregs_item.gprs.esp = sp;
 	    tag = startup_reply_tag();
	    append_gpregs_item();
	}


   
    void dump(debug_id_t id);

};


#endif /* !__L4KA__CXFER__MR_SAVE_H__ */
