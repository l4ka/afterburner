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
#include INC_ARCH(ia32.h)
#include INC_ARCH(page.h)
#include INC_ARCH(types.h)
#include INC_WEDGE(message.h)
#include INC_ARCH(hvm_vmx.h)

#define OFS_MR_SAVE_TAG		 0
#define OFS_MR_SAVE_PF_ADDR	 1
#define OFS_MR_SAVE_EXC_NO	 1
#define OFS_MR_SAVE_QUAL	 1
#define OFS_MR_SAVE_PF_IP	 2
#define OFS_MR_SAVE_ERRCODE	 2
#define OFS_MR_SAVE_ILEN	 2
#define OFS_MR_SAVE_AI_INFO	 3

#define OFS_MR_SAVE_CTRLXFER	(5 + sizeof(L4_TStateCtrlXferItem_t) / sizeof(L4_Word_t))
#define OFS_MR_SAVE_EIP		(L4_CTRLXFER_GPREGS_EIP    + OFS_MR_SAVE_CTRLXFER)
#define OFS_MR_SAVE_EFLAGS	(L4_CTRLXFER_GPREGS_EFLAGS + OFS_MR_SAVE_CTRLXFER) 
#define OFS_MR_SAVE_EDI		(L4_CTRLXFER_GPREGS_EDI    + OFS_MR_SAVE_CTRLXFER) 
#define OFS_MR_SAVE_ESI		(L4_CTRLXFER_GPREGS_ESI    + OFS_MR_SAVE_CTRLXFER)  
#define OFS_MR_SAVE_EBP		(L4_CTRLXFER_GPREGS_EBP    + OFS_MR_SAVE_CTRLXFER) 
#define OFS_MR_SAVE_ESP		(L4_CTRLXFER_GPREGS_ESP    + OFS_MR_SAVE_CTRLXFER) 
#define OFS_MR_SAVE_EBX		(L4_CTRLXFER_GPREGS_EBX    + OFS_MR_SAVE_CTRLXFER) 
#define OFS_MR_SAVE_EDX		(L4_CTRLXFER_GPREGS_EDX    + OFS_MR_SAVE_CTRLXFER) 
#define OFS_MR_SAVE_ECX		(L4_CTRLXFER_GPREGS_ECX    + OFS_MR_SAVE_CTRLXFER) 
#define OFS_MR_SAVE_EAX		(L4_CTRLXFER_GPREGS_EAX    + OFS_MR_SAVE_CTRLXFER) 

#define HVM_FAULT_LABEL(reason) ((msg_label_hvm_fault_end - (reason << 4)) & ~0xf)


enum thread_state_t { 
    thread_state_startup,
    thread_state_pfault,  
    thread_state_exception,
    thread_state_preemption,
    thread_state_activated,
};

class mrs_t
{
private:
    union 
    {
	L4_Word_t raw[0];
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
	    L4_TStateCtrlXferItem_t tstate_item;
	    L4_GPRegsCtrlXferItem_t gpr_item;
	};
    };

    static const char *gpreg_name[L4_CTRLXFER_GPREGS_SIZE];
#if defined(CONFIG_L4KA_HVM)
public:
    /* CR0 .. CR4 */
    L4_CRegsCtrlXferItem_t cr_item;
    L4_DRegsCtrlXferItem_t dr_item;
    /* CS, SS, DS, ES, FS, GS, TR, LDTR */
    L4_SegCtrlXferItem_t   seg_item[8];
    /* IDTR, GDTR */
    L4_DTRCtrlXferItem_t   dtr_item[2];
    L4_NonRegExcCtrlXferItem_t  nonregexc_item;
    L4_ExecCtrlXferItem_t execctrl_item;
    L4_OtherRegsCtrlXferItem_t  otherreg_item;
#endif
private:
    L4_Msg_t *msg;
    L4_Word_t mr;
    /* Flags */
    struct {
	bool vm8086;
    } flags;
    
public:
    
    L4_Word_t get(L4_Word_t idx)
	{
	    ASSERT(idx < (L4_CTRLXFER_GPREGS_SIZE+OFS_MR_SAVE_CTRLXFER));
	    return raw[idx];
	}
    
    void set(L4_Word_t idx, L4_Word_t val)
	{
	    ASSERT(idx < (L4_CTRLXFER_GPREGS_SIZE+OFS_MR_SAVE_CTRLXFER));
	    raw[idx] = val;
	}

    void append_gpr_item() 
	{ 
	    L4_Init(&gpr_item.item, L4_CTRLXFER_GPREGS_ID); 
	    gpr_item.item.mask = 0x3ff;
	}

    void append_gpr_item(L4_Word_t gp, L4_Word_t val, bool c=false) 
	{ 
	    ASSERT(gp < L4_CTRLXFER_GPREGS_SIZE);
	    ASSERT((gpr_item.item.mask & (1 << gp)) == 0);
	    L4_Set(&gpr_item, gp, val);
	    gpr_item.item.C=c;
	}

    void store_tstate_item()
	{
	    mr += L4_Store(msg, mr, &tstate_item);
	    tstate_item.item.mask = 0;
	}
    
    void store_gpr_item()
	{
	    mr += L4_Store(msg, mr, &gpr_item);
	    gpr_item.item.mask = 0;
	}
    
    void load_gpr_item()
	{
	    /* GPRegs CtrlXfer Item */
	    L4_Append(msg, &gpr_item);
	    gpr_item.item.mask = 0;
	}


#if defined(CONFIG_L4KA_HVM)
    void append_cr_item(L4_Word_t cr, L4_Word_t val, bool c=false) 
	{
	    ASSERT(cr < L4_CTRLXFER_CREGS_SIZE);
	    ASSERT((cr_item.item.mask & (1 << cr)) == 0);
	    L4_Set(&cr_item, cr, val);
	    cr_item.item.C=c;
	}

    void load_cr_item()
	{
	    /* CRRegs CtrlXfer Item */
	    L4_Append(msg, &cr_item);
	    cr_item.item.mask = 0;
	}

    void append_dr_item(L4_Word_t dr, L4_Word_t val, bool c=false) 
	{
	    ASSERT(dr < L4_CTRLXFER_DREGS_SIZE);
	    ASSERT((dr_item.item.mask & (1 << dr)) == 0);
	    L4_Set(&dr_item, dr, val);
	    dr_item.item.C=c;
	}
    
    void load_dr_item()
	{
	    /* DRegs CtrlXfer Item */
	    L4_Append(msg, &dr_item);
	    dr_item.item.mask = 0;
	}

    void append_seg_item(L4_Word_t id, L4_Word_t sel, L4_Word_t base, L4_Word_t limit, L4_Word_t attr, bool c=false) 
	{
	    ASSERT(id >= L4_CTRLXFER_CSREGS_ID && id <= L4_CTRLXFER_LDTRREGS_ID);
	    ASSERT(seg_item[id-L4_CTRLXFER_CSREGS_ID].item.mask == 0);
	    L4_Init(&seg_item[id-L4_CTRLXFER_CSREGS_ID], id);
	    L4_Set(&seg_item[id-L4_CTRLXFER_CSREGS_ID], 0, sel);
	    L4_Set(&seg_item[id-L4_CTRLXFER_CSREGS_ID], 1, base);
	    L4_Set(&seg_item[id-L4_CTRLXFER_CSREGS_ID], 2, limit);
	    L4_Set(&seg_item[id-L4_CTRLXFER_CSREGS_ID], 3, attr);
	    seg_item[id-L4_CTRLXFER_CSREGS_ID].item.C=c;
	}
    
    void store_seg_item(L4_Word_t id)
	{
	    ASSERT(id >= L4_CTRLXFER_CSREGS_ID && id <= L4_CTRLXFER_LDTRREGS_ID);
	    mr += L4_Store(msg, mr, &seg_item[id-L4_CTRLXFER_CSREGS_ID]);
	    seg_item[id-L4_CTRLXFER_CSREGS_ID].item.mask = 0;

	}
    
    void load_seg_item(L4_Word_t id)
	{
	    ASSERT(id >= L4_CTRLXFER_CSREGS_ID && id <= L4_CTRLXFER_LDTRREGS_ID);
	    L4_Append(msg, &seg_item[id-L4_CTRLXFER_CSREGS_ID]);
	    seg_item[id-L4_CTRLXFER_CSREGS_ID].item.mask = 0;
	}
    
    void append_dtr_item(L4_Word_t id, L4_Word_t base, L4_Word_t limit, bool c=false) 
	{
	    ASSERT(id >= L4_CTRLXFER_IDTRREGS_ID && id <= L4_CTRLXFER_GDTRREGS_ID);
	    ASSERT(dtr_item[id-L4_CTRLXFER_IDTRREGS_ID].item.mask == 0);
	    L4_Init(&dtr_item[id-L4_CTRLXFER_IDTRREGS_ID], id);
	    L4_Set(&dtr_item[id-L4_CTRLXFER_IDTRREGS_ID], 0, base);
	    L4_Set(&dtr_item[id-L4_CTRLXFER_IDTRREGS_ID], 1, limit);
	    dtr_item[id-L4_CTRLXFER_IDTRREGS_ID].item.C=c;
	}
    
    void store_dtr_item(L4_Word_t id)
	{
	    ASSERT(id >= L4_CTRLXFER_IDTRREGS_ID && id <= L4_CTRLXFER_GDTRREGS_ID);
	    mr += L4_Store(msg, mr, &dtr_item[id-L4_CTRLXFER_IDTRREGS_ID]);
	    dtr_item[id-L4_CTRLXFER_IDTRREGS_ID].item.mask = 0;

	}
    
    void load_dtr_item(L4_Word_t id)
	{
	    ASSERT(id >= L4_CTRLXFER_IDTRREGS_ID && id <= L4_CTRLXFER_GDTRREGS_ID);
	    L4_Append(msg, &dtr_item[id-L4_CTRLXFER_IDTRREGS_ID]);
	    dtr_item[id-L4_CTRLXFER_IDTRREGS_ID].item.mask = 0;
	}
    
    void append_nonregexc_item(L4_Word_t nr, L4_Word_t val, bool c=false) 
	{
	    ASSERT(nr < L4_CTRLXFER_NONREGEXC_SIZE);
	    ASSERT((nonregexc_item.item.mask & (1 << nr)) == 0);
	    L4_Set(&nonregexc_item, nr, val);
	    nonregexc_item.item.C=c;
	}

    void append_exc_item(L4_Word_t exc, L4_Word_t eec, L4_Word_t ilen, bool c=false) 
	{
	    ASSERT((nonregexc_item.item.mask & (1 << L4_CTRLXFER_NONREGEXC_ENT_INFO)) == 0);
	    ASSERT((nonregexc_item.item.mask & (1 << L4_CTRLXFER_NONREGEXC_ENT_EEC)) == 0);
	    ASSERT((nonregexc_item.item.mask & (1 << L4_CTRLXFER_NONREGEXC_ENT_ILEN)) == 0);
	    
	    L4_Set(&nonregexc_item, L4_CTRLXFER_NONREGEXC_ENT_INFO, exc);
	    L4_Set(&nonregexc_item, L4_CTRLXFER_NONREGEXC_ENT_EEC, eec);
	    L4_Set(&nonregexc_item, L4_CTRLXFER_NONREGEXC_ENT_ILEN, ilen);
	    nonregexc_item.item.C=c;
	}

    void load_nonregexc_item()
	{
	    L4_Append(msg, &nonregexc_item);
	    nonregexc_item.item.mask = 0;
	}

    void store_nonregexc_item()
	{
	    mr += L4_Store(msg, mr, &nonregexc_item);
	    nonregexc_item.item.mask = 0;
	}

    
    void append_execctrl_item(L4_Word_t ex, L4_Word_t val, bool c=false) 
	{
	    ASSERT(ex < L4_CTRLXFER_EXECCTRL_SIZE);
	    ASSERT((execctrl_item.item.mask & (1 << ex)) == 0);
	    L4_Set(&execctrl_item, ex, val);
	    execctrl_item.item.C=c;
	}

    void load_execctrl_item()
	{
	    L4_Append(msg, &execctrl_item);
	    execctrl_item.item.mask = 0;
	}

    void store_excecctrl_item()
	{
	    mr += L4_Store(msg, mr, &execctrl_item );
	    execctrl_item.item.mask = 0;
	}
    
    void append_otherreg_item(L4_Word_t ori, L4_Word_t val, bool c=false) 
	{
	    ASSERT(ori < L4_CTRLXFER_OTHERREGS_SIZE);
	    ASSERT((otherreg_item.item.mask & (1 << ori)) == 0);
	    L4_Set(&otherreg_item, ori, val);
	    otherreg_item.item.C=c;
	}
    
    void load_otherreg_item()
	{
	    L4_Append(msg, &otherreg_item);
	    L4_Init(&otherreg_item);
	    otherreg_item.item.mask = 0;
	}
    
    static L4_Word_t hvm_to_gpreg(L4_Word_t hvm_reg)
	{ 
	    ASSERT(hvm_reg < L4_CTRLXFER_GPREGS_SIZE);
	    return L4_CTRLXFER_GPREGS_SIZE - hvm_reg - 1;
	}
#endif

    void init()
	{ 
	    tag.raw = 0;
	    L4_Init(&gpr_item); 
#if defined(CONFIG_L4KA_HVM)
	    L4_Init(&cr_item);
	    L4_Init(&dr_item);
	    for (L4_Word_t seg=0; seg<8; seg++)
		L4_Init(&seg_item[seg], L4_CTRLXFER_CSREGS_ID + seg);
	    for (L4_Word_t seg=0; seg<2; seg++)
		L4_Init(&dtr_item[seg], L4_CTRLXFER_IDTRREGS_ID + seg);
	    L4_Init(&nonregexc_item);
	    L4_Init(&execctrl_item);
	    L4_Init(&otherreg_item); 
#endif
	}
    
    void init_msg(bool init_tag = true) 
	{
	    if (init_tag) L4_Set_MsgTag(L4_Niltag);
	    msg = (L4_Msg_t *) __L4_X86_Utcb (); 
	}
    
    void store() 
	{	
	    init_msg(false);
	    
	    tag.raw = msg->raw[0];
	    
	    ASSERT (tag.X.u <= 3);
    
	    for (mr = 1; mr < tag.X.u+1; mr++)
		raw[mr] = msg->raw[mr];
	    
	    dprintf(debug_task+1, "store mrs <%d %d: %x:%x:%x>",
		    tag.X.u, tag.X.t, raw[0], raw[1], raw[2]);
	    
	    if (tag.X.t)
	    {
	    
		store_tstate_item();
		store_gpr_item();

#if defined(CONFIG_L4KA_HVM)
		if (flags.vm8086)
		{
		    store_seg_item(L4_CTRLXFER_CSREGS_ID);
		    store_seg_item(L4_CTRLXFER_SSREGS_ID);
		}
	    
		switch (t.X.label)
		{
		case HVM_FAULT_LABEL(hvm_vmx_reason_io):
		    store_seg_item(L4_CTRLXFER_DSREGS_ID);
		    store_seg_item(L4_CTRLXFER_ESREGS_ID);
		    break;
		default:
		    break;
		}
		store_nonregexc_item();
#endif
	    }
	    
	    ASSERT(mr == 1 + tag.X.t + tag.X.u);
	    dprintf(debug_task+1, "store mrs");
	    dump(debug_task+1, true);


	}
    
    void load(word_t additional_untyped=0) 
	{	
	    init_msg();

	    dprintf(debug_task+1, "load mrs");
	    dump(debug_task+1, true);
	    
	    /* Tag */
	    L4_LoadMR ( 0, tag.raw);
	    
	    /* map item, if needed */
	    L4_LoadMRs( 1 + tag.X.u, tag.X.t, pfault.item.raw );
	    

	    /* Untyped words (max 2 + additional_untyped) */
	    ASSERT(tag.X.u <= 2);
	    tag.X.u += additional_untyped;
	    L4_LoadMRs( 1 + additional_untyped, tag.X.u - additional_untyped, untyped);	

	    /* GPReg CtrlXfer Item */
	    load_gpr_item();

#if defined(CONFIG_L4KA_HVM)
	    /* CR Item */
	    load_cr_item();

	    /* DR Item */
	    load_dr_item();

	    /* Seg Items */
	    for (word_t id=L4_CTRLXFER_CSREGS_ID; id<=L4_CTRLXFER_LDTRREGS_ID; id++)
		load_seg_item(id);
	    
	    /* DTR Items */
	    for (word_t id=L4_CTRLXFER_IDTRREGS_ID; id<=L4_CTRLXFER_GDTRREGS_ID; id++)
		load_dtr_item(id);

	    /* Nonreg/Exc CtrlXfer Item */
	    load_nonregexc_item();


	    /* Exec CtrlXfer Item */
	    load_execctrl_item();
	    
	    /* OtherReg CtrlXfer Item */
	    load_otherreg_item();
#endif
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
	    return (L4_Label(tag) == msg_label_preemption);
	}
    
    bool is_activation_msg() 
	{ 
	    return (L4_Label(tag) == msg_label_preemption_activation);
	}
    
    bool is_yield_msg() 
	{ 
	    return (L4_Label(tag) == msg_label_preemption_yield);
	}

    bool is_pfault_msg() 
	{ 
	    return (L4_Label(tag) >= msg_label_pfault_start &&
		    L4_Label(tag) <= msg_label_pfault_end);
	}
    
    L4_Word_t get_pfault_ip() { return gpr_item.regs.eip; }
    L4_Word_t get_pfault_addr() { return pfault.addr; }
    L4_Word_t get_pfault_rwx() { return L4_Label(tag) & 0x7; }

    L4_Word_t get_exc_ip() { return gpr_item.regs.eip; }
    void set_exc_ip(word_t ip) { gpr_item.regs.eip = ip; }
    L4_Word_t get_exc_sp() { return gpr_item.regs.esp; }
    L4_Word_t get_exc_number() { return exception.excno; }

    L4_Word64_t get_preempt_time() 
	{ return ((L4_Word64_t) preempt.time2 << 32) | ((L4_Word64_t) preempt.time1); }
    L4_Word_t get_preempt_ip() 
	{ return gpr_item.regs.eip; }
    L4_ThreadId_t get_preempt_target() 
	{ return (L4_ThreadId_t) { raw : gpr_item.regs.eax }; }
    
    void load_iret_emul_frame(iret_handler_frame_t *frame)
	{
	    for( u32_t i = 0; i < 9; i++ )
		gpr_item.regs.reg[i+1] = frame->frame.x.raw[8-i];	
	    
	    gpr_item.regs.eflags = frame->iret.flags.x.raw;
	    gpr_item.regs.eip = frame->iret.ip;
	    gpr_item.regs.esp = frame->iret.sp;
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
		
	    append_gpr_item();
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
	    
	    append_gpr_item();
	    dump(debug_exception+1);
	}
    
    
    static const L4_MsgTag_t startup_reply_tag()
	{ return (L4_MsgTag_t) { X: { 0, 0, 0, msg_label_startup_reply} } ;}

    void load_startup_reply(iret_handler_frame_t *iret_emul_frame) 
	{ 
	    load_iret_emul_frame(iret_emul_frame);
	    tag = startup_reply_tag();
	    append_gpr_item();
	    dump(debug_task+1);
	}
    
    void load_startup_reply(L4_Word_t ip, L4_Word_t sp) 
	{ 
	    gpr_item.regs.eip = ip;
	    gpr_item.regs.esp = sp;
 	    tag = startup_reply_tag();
	    append_gpr_item();
	}

   
    static const L4_MsgTag_t preemption_continue_tag()
	{ return (L4_MsgTag_t) { X: { 0, 0, 0, msg_label_preemption_continue} }; }

    
    void load_preemption_reply(bool cxfer, iret_handler_frame_t *iret_emul_frame=NULL) 
	{ 
	    if (iret_emul_frame)
		load_iret_emul_frame(iret_emul_frame);
	    
	    tag = preemption_continue_tag();
	    if (cxfer) 
		append_gpr_item();
	    dump(debug_preemption+1);
	}

    static L4_MsgTag_t yield_tag()
	{ return (L4_MsgTag_t) { X: { 2, 0, 0, msg_label_preemption_yield} } ;}

    void load_yield_msg(L4_ThreadId_t dest, bool cxfer=true) 
	{ 
	    tag = yield_tag();
	    if (cxfer)
	    {
		gpr_item.regs.eax = dest.raw;
		append_gpr_item();
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
    bool is_hvm_fault_msg() 
	{ 
	    return (L4_Label(tag) >= msg_label_hvm_fault_start &&
		    L4_Label(tag) <= msg_label_hvm_fault_end);
	}
    
    L4_Word_t get_hvm_fault_reason() 
	{ 
	    ASSERT(is_hvm_fault_msg());
	    return (msg_label_hvm_fault_end >> 4) - (L4_Label(tag) >> 4);
	}

    bool is_hvm_fault_internal()
	{   return (L4_Label(tag) & 0x8); }
	
    static L4_MsgTag_t vfault_reply()
	{ return (L4_MsgTag_t) { X: { 0, 0, 0, msg_label_hvm_fault_reply} } ;}

    void load_vfault_reply(bool next_eip = true) 
	{ 
	    tag = vfault_reply();

	    append_gpr_item();
	    dump(debug_hvm_fault+1);
	}
    
    void next_instruction()
	{
	    gpr_item.regs.eip += hvm.ilen;
	    // Disable interrupt blocking if set
	    if (nonregexc_item.regs.ias & 0x3)
		append_nonregexc_item(L4_CTRLXFER_NONREGEXC_INT, nonregexc_item.regs.ias & ~0x3);
	}
    
    void set_vm8086(bool v) { flags.vm8086 = v; }
#endif


    void dump(debug_id_t id, bool extended=false);
    static const char  *regname(L4_Word_t reg)
	{ 
	    ASSERT(reg < L4_CTRLXFER_GPREGS_SIZE);
	    return gpreg_name[reg];
	}
    static const word_t regnameword(L4_Word_t reg)
	{ 
	    ASSERT(reg < L4_CTRLXFER_GPREGS_SIZE);
	    return * (word_t *) gpreg_name[reg];
	}
    

};


#endif /* !__L4KA__CXFER__MR_SAVE_H__ */
