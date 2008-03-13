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
#include <console.h>
#include <l4/ia32/arch.h>
#include <l4/ipc.h>
#include INC_ARCH(cpu.h)
#include INC_ARCH(page.h)
#include INC_ARCH(types.h)
#include INC_WEDGE(message.h)

#define OFS_MR_SAVE_TAG		 0
#define OFS_MR_SAVE_PF_ADDR	 1
#define OFS_MR_SAVE_EXC_NO	 1
#define OFS_MR_SAVE_QUAL	 1
#define OFS_MR_SAVE_PF_IP	 2
#define OFS_MR_SAVE_ERRCODE	 2
#define OFS_MR_SAVE_ILEN	 2
#define OFS_MR_SAVE_AI_INFO	 3

#define OFS_MR_SAVE_CTRLXFER				    (4)
#define OFS_MR_SAVE_EIP		(L4_CTRLXFER_GPREGS_EIP    + 5)
#define OFS_MR_SAVE_EFLAGS	(L4_CTRLXFER_GPREGS_EFLAGS + 5) 
#define OFS_MR_SAVE_EDI		(L4_CTRLXFER_GPREGS_EDI    + 5) 
#define OFS_MR_SAVE_ESI		(L4_CTRLXFER_GPREGS_ESI    + 5)  
#define OFS_MR_SAVE_EBP		(L4_CTRLXFER_GPREGS_EBP    + 5) 
#define OFS_MR_SAVE_ESP		(L4_CTRLXFER_GPREGS_ESP    + 5) 
#define OFS_MR_SAVE_EBX		(L4_CTRLXFER_GPREGS_EBX    + 5) 
#define OFS_MR_SAVE_EDX		(L4_CTRLXFER_GPREGS_EDX    + 5) 
#define OFS_MR_SAVE_ECX		(L4_CTRLXFER_GPREGS_ECX    + 5) 
#define OFS_MR_SAVE_EAX		(L4_CTRLXFER_GPREGS_EAX    + 5) 

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
	L4_Word_t raw[L4_CTRLXFER_GPREGS_SIZE+5];
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
		struct {
		    L4_Word_t qual;
		    L4_Word_t ilen;
		    L4_Word_t ai_info;
		} hvm;
		L4_Word_t untyped[3];
	    };
	    L4_GPRegsCtrlXferItem_t gpregs_item;
	};
    };

    static const char *gpreg_name[L4_CTRLXFER_GPREGS_SIZE];
#if defined(CONFIG_L4KA_HVM)
public:
    /* CR0 .. CR4 */
    L4_CRegsCtrlXferItem_t cr_item;
    L4_CRegsCtrlXferItem_t dr_item;
    /* CS, SS, DS, ES, FS, GS, TR, LDTR, GDTR, IDTR */
    L4_SegCtrlXferItem_t   seg_item[10];
    L4_NonRegCtrlXferItem_t  nonreg_item;
    L4_ExcCtrlXferItem_t exc_item;
    L4_ExcCtrlXferItem_t exec_item;
#endif

   
public:
    
    L4_Word_t get(word_t idx)
	{
	    ASSERT(idx < (L4_CTRLXFER_GPREGS_SIZE+5));
	    return raw[idx];
	}
    void set(word_t idx, word_t val)
	{
	    ASSERT(idx < (L4_CTRLXFER_GPREGS_SIZE+5));
	    raw[idx] = val;
	}

    void append_gpregs_item() 
	{ 
	    L4_Init(&gpregs_item.item, L4_CTRLXFER_GPREGS_ID); 
	    gpregs_item.item.num_regs = L4_CTRLXFER_GPREGS_SIZE;
	    gpregs_item.item.mask = 0x3ff;
	}

#if defined(CONFIG_L4KA_HVM)
    void append_cr_item(word_t cr, word_t val, bool c=false) 
	{
	    ASSERT(cr < L4_CTRLXFER_CREGS_SIZE);
	    L4_Set(&cr_item, cr, val);
	    cr_item.item.C=c;
	}

    void append_dr_item(word_t dr, word_t val, bool c=false) 
	{
	    ASSERT(dr < L4_CTRLXFER_DREGS_SIZE);
	    L4_Set(&dr_item, dr, val);
	    dr_item.item.C=c;
	}

    void append_seg_item(word_t id, word_t sel, word_t base, word_t limit, word_t attr, bool c=false) 
	{
	    ASSERT(id >= L4_CTRLXFER_CSREGS_ID && id <= L4_CTRLXFER_GDTRREGS_ID);
	    L4_Init(&seg_item[id-L4_CTRLXFER_CSREGS_ID], id);
	    L4_Set(&seg_item[id-L4_CTRLXFER_CSREGS_ID], 0, sel);
	    L4_Set(&seg_item[id-L4_CTRLXFER_CSREGS_ID], 1, base);
	    L4_Set(&seg_item[id-L4_CTRLXFER_CSREGS_ID], 2, limit);
	    L4_Set(&seg_item[id-L4_CTRLXFER_CSREGS_ID], 3, attr);
	    seg_item[id-L4_CTRLXFER_CSREGS_ID].item.C=c;
	}
    
    void append_exc_item(word_t exc_info, word_t err_code, word_t ilen, bool c=false)
	{
	    L4_Init(&exc_item);
	    L4_Set(&exc_item, exc_info, err_code, ilen);
	    exc_item.item.C=c;
	}
    
    void append_nonreg_item(word_t nr, word_t val, bool c=false) 
	{
	    ASSERT(nr < L4_CTRLXFER_NONREGS_SIZE);
	    L4_Set(&nonreg_item, nr, val);
	    nonreg_item.item.C=c;
	}


    
    static word_t hvm_to_gpreg(word_t hvm_reg)
	{ 
	    ASSERT(hvm_reg < L4_CTRLXFER_GPREGS_SIZE);
	    return L4_CTRLXFER_GPREGS_SIZE - hvm_reg - 1;
	}
#endif

    void init()
	{ 
	    tag.raw = 0;
	    L4_Init(&gpregs_item); 
#if defined(CONFIG_L4KA_HVM)
	    L4_Init(&cr_item);
	    L4_Init(&dr_item);
	    for (word_t seg=0; seg<10; seg++)
		L4_Init(&seg_item[seg], L4_CTRLXFER_CSREGS_ID + seg);
	    L4_Init(&exc_item); 
	    L4_Init(&exec_item);
#endif
	}
    
    void store(L4_MsgTag_t t) 
	{	
	    switch (t.X.label)
	    {
	    case msg_label_exception:
	    case msg_label_pfault_start ... msg_label_pfault_end:
	    case msg_label_preemption ... msg_label_preemption_yield:
	    case msg_label_hvm_fault_start ... msg_label_hvm_fault_end:
		
		ASSERT (t.X.u <= 3);
		ASSERT (t.X.t == L4_CTRLXFER_GPREGS_SIZE+1);
		
		L4_StoreMRs( 0, t.X.u, raw);
		L4_StoreMRs( t.X.u+1, L4_CTRLXFER_GPREGS_SIZE+1, gpregs_item.raw );
		/* Reset num_regs, since it's used as write indicator */
		gpregs_item.item.num_regs = 0;
		break;
	    default:
		ASSERT(false);
		break;
	    }
	}
    
    void load(word_t additional_untyped=0) 
	{	
	    tag.X.u += additional_untyped;
	    
	    /* map item, if needed */
	    L4_LoadMRs( 1 + tag.X.u, tag.X.t, pfault.item.raw );
	    
	    /* GP CtrlXfer Item */
	    if (gpregs_item.item.num_regs)
	    {
		ASSERT(gpregs_item.item.num_regs == L4_CTRLXFER_GPREGS_SIZE);
		L4_LoadMRs(1 + tag.X.u + tag.X.t, 1 + gpregs_item.item.num_regs, gpregs_item.raw);
		tag.X.t += 1 + gpregs_item.item.num_regs;
		gpregs_item.item.num_regs = 0;
	    }
	    	    
	    /* Builtin untyped words (max 2) */
	    ASSERT(tag.X.u - additional_untyped <= 2);
	    L4_LoadMRs( 1 + additional_untyped, tag.X.u - additional_untyped, untyped);	
	    
	    /* Tag */
	    L4_LoadMR ( 0, tag.raw);
	    
#if defined(CONFIG_L4KA_HVM)
	    L4_Msg_t *msg = (L4_Msg_t *) __L4_X86_Utcb ();

	    /* CR Item */
	    L4_Append(msg, &cr_item);
	    cr_item.item.num_regs = 0;
	    cr_item.item.mask = 0;

	    /* DR Item */
	    L4_Append(msg, &dr_item);
	    dr_item.item.num_regs = 0;
	    dr_item.item.mask = 0;

	    /* Seg Items */
	    for (word_t seg=0; seg<10; seg++)
	    {
		L4_Append(msg, &seg_item[seg]);
		seg_item[seg].item.num_regs = 0;
		seg_item[seg].item.mask = 0;
	    }

	    /* Nonreg CtrlXfer Item */
	    L4_Append(msg, &nonreg_item);
	    nonreg_item.item.num_regs = 0;
	    nonreg_item.item.mask = 0;
	    
	    /* Exception CtrlXfer Item */
	    L4_Append(msg, &exc_item);
	    exc_item.item.num_regs = 0;
	    exc_item.item.mask = 0;

	    /* Exec CtrlXfer Item */
	    L4_Append(msg, &exec_item);
	    exec_item.item.num_regs = 0;
	    exec_item.item.mask = 0;
#endif
	    clear_msg_tag();
	}


    L4_MsgTag_t get_msg_tag() { return tag; }
    L4_Word_t get_hvm_reason() { return (msg_label_hvm_fault_end - L4_Label(tag)) >> 4 ; }

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
    
    L4_Word_t get_pfault_ip() { return gpregs_item.regs.eip; }
    L4_Word_t get_pfault_addr() { return pfault.addr; }
    L4_Word_t get_pfault_rwx() { return L4_Label(tag) & 0x7; }

    L4_Word_t get_exc_ip() { return gpregs_item.regs.eip; }
    void set_exc_ip(word_t ip) { gpregs_item.regs.eip = ip; }
    L4_Word_t get_exc_sp() { return gpregs_item.regs.esp; }
    L4_Word_t get_exc_number() { return exception.excno; }

    L4_Word64_t get_preempt_time() 
	{ return ((L4_Word64_t) preempt.time2 << 32) | ((L4_Word64_t) preempt.time1); }
    L4_Word_t get_preempt_ip() 
	{ return gpregs_item.regs.eip; }
    L4_ThreadId_t get_preempt_target() 
	{ return (L4_ThreadId_t) { raw : gpregs_item.regs.eax }; }
   
    void load_iret_emul_frame(iret_handler_frame_t *frame)
	{
	    for( u32_t i = 0; i < 9; i++ )
		gpregs_item.regs.reg[i+1] = frame->frame.x.raw[8-i];	
	    
	    gpregs_item.regs.eflags = frame->iret.flags.x.raw;
	    gpregs_item.regs.eip = frame->iret.ip;
	    gpregs_item.regs.esp = frame->iret.sp;
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
    
    void load_startup_reply(L4_Word_t ip, L4_Word_t sp) 
	{ 
	    gpregs_item.regs.eip = ip;
	    gpregs_item.regs.esp = sp;
 	    tag = startup_reply_tag();
	    append_gpregs_item();
	}

   
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
		gpregs_item.regs.eax = dest.raw;
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
    
#if defined(CONFIG_L4KA_HVM)
    void load_startup_reply(word_t ip, word_t sp, word_t cs, word_t ss, bool real_mode);

    static L4_MsgTag_t vfault_reply()
	{ return (L4_MsgTag_t) { X: { 0, 0, 0, msg_label_hvm_fault_reply} } ;}

    void load_vfault_reply(bool next_eip = true) 
	{ 
	    tag = vfault_reply();
	    gpregs_item.regs.eip += hvm.ilen;

	    append_gpregs_item();
	    dump(debug_hvm_fault+1);
	}
#endif


    void dump(debug_id_t id, bool extended=false);
    static const char *regname(word_t reg)
	{ 
	    ASSERT(reg < L4_CTRLXFER_GPREGS_SIZE);
	    return gpreg_name[reg];
	}
    

};


#endif /* !__L4KA__CXFER__MR_SAVE_H__ */