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

#define IA32_EXC_DIVIDE_ERROR		0
#define IA32_EXC_DEBUG			1
#define IA32_EXC_NMI			2
#define IA32_EXC_BREAKPOINT		3
#define IA32_EXC_OVERFLOW		4
#define IA32_EXC_BOUNDRANGE		5
#define IA32_EXC_INVALIDOPCODE		6
#define IA32_EXC_NOMATH_COPROC		7
#define IA32_EXC_DOUBLEFAULT		8
#define IA32_EXC_COPSEG_OVERRUN		9
#define IA32_EXC_INVALID_TSS		10
#define IA32_EXC_SEGMENT_NOT_PRESENT	11
#define IA32_EXC_STACKSEG_FAULT		12
#define IA32_EXC_GENERAL_PROTECTION	13
#define IA32_EXC_PAGEFAULT		14
#define IA32_EXC_RESERVED		15
#define IA32_EXC_FPU_FAULT		16
#define IA32_EXC_ALIGNEMENT_CHECK	17
#define IA32_EXC_MACHINE_CHECK		18
#define IA32_EXC_SIMD_FAULT		19

/* Intel reserved exceptions */
#define IA32_EXC_RESERVED_FIRST		20
#define IA32_EXC_RESERVED_LAST		31

#define OFS_MR_SAVE_TAG		 0
#define OFS_MR_SAVE_PF_ADDR	 1
#define OFS_MR_SAVE_EXC_NO	 1
#define OFS_MR_SAVE_PF_IP	 1
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
		L4_Word_t untyped[2];
	    };
	    L4_CtrlXferItem_t ctrlxfer;
	    bool fpu_state_valid;
	    L4_Word_t fpu_state[fpu_state_size];
	};
    };
public:

    void init(){ tag.raw = 0; fpu_state_valid = false; }
    
    L4_Word_t get(word_t idx)
	{
	    ASSERT(idx < (CTRLXFER_SIZE+3));
	    return raw[idx];
	}
    void set(word_t idx, word_t val)
	{
	    ASSERT(idx < (CTRLXFER_SIZE+3));
	    raw[idx] = val;
	}

    void store_mrs(L4_MsgTag_t t) 
	{	
	    if (L4_UntypedWords(t) != 2)
		L4_KDB_Enter("ASSERT STORE_MRS");
	    ASSERT (L4_UntypedWords(t) == 2);
	    ASSERT (L4_TypedWords(t) == CTRLXFER_SIZE);
	    L4_StoreMR( 0, &raw[0] );
	    L4_StoreMR( 1, &raw[1] );
	    L4_StoreMR( 2, &raw[2] );
	    L4_StoreCtrlXferItem(3, &ctrlxfer);
	    if (ctrlxfer.tag.X.fpu)
	    {
		L4_StoreMRs( 3 + CTRLXFER_SIZE, fpu_state_size, fpu_state );
		fpu_state_valid = true;
	    }
	}
    void load(word_t additional_untyped=0) 
	{	    
	    tag.X.u += additional_untyped;
	    word_t mapitems = is_pfault_msg() ? 2 : 0;
	    
	    if ((ctrlxfer.tag.X.fpu == CTRLXFER_FPU_ENABLE) && fpu_state_valid)
	    {
		L4_LoadMRs( 1 + L4_UntypedWords(tag) + L4_TypedWords(tag), fpu_state_size, fpu_state );
		L4_SetCtrlXferMask(&ctrlxfer, 0x3ff, CTRLXFER_FPU_LOADSTORE | CTRLXFER_FPU_ENABLE);
		tag.X.t += fpu_state_size;
	    }
	    L4_LoadMRs( 1 + L4_UntypedWords(tag) + mapitems, L4_TypedWords(tag)-mapitems, ctrlxfer.raw );
	    L4_LoadMRs( 1 + L4_UntypedWords(tag), mapitems, pfault.item.raw );
	    L4_LoadMRs( 1 + additional_untyped, L4_UntypedWords(tag), untyped);
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
    
    L4_Word_t get_pfault_ip() { return ctrlxfer.eip; }
    L4_Word_t get_pfault_addr() { return pfault.addr; }
    L4_Word_t get_pfault_rwx() { return L4_Label(tag) & 0x7; }

    L4_Word_t get_exc_ip() { return ctrlxfer.eip; }
    void set_exc_ip(word_t ip) { ctrlxfer.eip = ip; }
    L4_Word_t get_exc_sp() { return ctrlxfer.esp; }
    L4_Word_t get_exc_number() { return exception.excno; }

    L4_Word64_t get_preempt_time() 
	{ return ((L4_Word64_t) preempt.time2 << 32) | ((L4_Word64_t) preempt.time1); }
    L4_Word_t get_preempt_ip() 
	{ return ctrlxfer.eip; }
    L4_ThreadId_t get_preempt_target() 
	{ return (L4_ThreadId_t) { raw : ctrlxfer.eax }; }
   

    static const L4_MsgTag_t pfault_reply_tag()
	{ return (L4_MsgTag_t) { X: { 0, 2 + CTRLXFER_SIZE, 0, msg_label_pfault_start} } ;}


    void load_pfault_reply(L4_MapItem_t map_item, iret_handler_frame_t *iret_emul_frame=NULL) 
	{
	    ASSERT(is_pfault_msg());
	    tag = pfault_reply_tag();
	    pfault.item = map_item;
	    if (iret_emul_frame)
	    {
		for( u32_t i = 0; i < 9; i++ )
		    ctrlxfer.regs[i+1] = iret_emul_frame->frame.x.raw[8-i];	
		ctrlxfer.eflags = iret_emul_frame->iret.flags.x.raw;
		ctrlxfer.eip = iret_emul_frame->iret.ip;
		ctrlxfer.esp = iret_emul_frame->iret.sp;
	    }
	    L4_SetCtrlXferMask(&ctrlxfer, 0x3ff, CTRLXFER_FPU_ZERO);
	}
    
    static const L4_MsgTag_t exc_reply_tag()
	{ return (L4_MsgTag_t) { X: { 0, CTRLXFER_SIZE, 0, msg_label_exception} } ;}
    

    void load_exception_reply(bool enable_fpu, iret_handler_frame_t *iret_emul_frame) 
	{
	    ASSERT(is_exception_msg());
	    tag = exc_reply_tag();
	    
	    if (iret_emul_frame)
	    {
		for( u32_t i = 0; i < 9; i++ )
		    ctrlxfer.regs[i+1] = iret_emul_frame->frame.x.raw[8-i];	
		ctrlxfer.eflags = iret_emul_frame->iret.flags.x.raw;
		ctrlxfer.eip = iret_emul_frame->iret.ip;
		ctrlxfer.esp = iret_emul_frame->iret.sp;
	    }
	    L4_SetCtrlXferMask(&ctrlxfer, 0x3ff, enable_fpu ? CTRLXFER_FPU_ENABLE : CTRLXFER_FPU_ZERO);
	}
    
    
    static const L4_MsgTag_t startup_reply_tag()
	{ return (L4_MsgTag_t) { X: { 0, CTRLXFER_SIZE, 0, msg_label_startup_reply} } ;}

    void load_startup_reply(iret_handler_frame_t *iret_emul_frame) 
	{ 
	    for( u32_t i = 0; i < 9; i++ )
		ctrlxfer.regs[i+1] = iret_emul_frame->frame.x.raw[8-i];
	    ctrlxfer.eflags = iret_emul_frame->iret.flags.x.raw;
	    ctrlxfer.eip = iret_emul_frame->iret.ip;
	    ctrlxfer.esp = iret_emul_frame->iret.sp;
	    
	    tag = startup_reply_tag();
	    L4_SetCtrlXferMask(&ctrlxfer, 0x3ff, CTRLXFER_FPU_ZERO);
	}

    void load_startup_reply(L4_Word_t ip, L4_Word_t sp) 
	{ 
	    ctrlxfer.eip = ip;
	    ctrlxfer.esp = sp;
    
	    tag = startup_reply_tag();
	    L4_SetCtrlXferMask(&ctrlxfer, 0x3ff, CTRLXFER_FPU_ZERO);
	}

    static const L4_MsgTag_t preemption_reply_tag(const bool cxfer)
	{ return (L4_MsgTag_t) { X: { 0, (cxfer ? CTRLXFER_SIZE : 0), 0, msg_label_preemption_reply} }; }


    void load_preemption_reply(bool cxfer, iret_handler_frame_t *iret_emul_frame=NULL) 
	{ 
	    ASSERT(is_preemption_msg());
	    
	    if (iret_emul_frame)
	    {
		for( u32_t i = 0; i < 9; i++ )
		    ctrlxfer.regs[i+1] = iret_emul_frame->frame.x.raw[8-i];
		ctrlxfer.eflags = iret_emul_frame->iret.flags.x.raw;
		ctrlxfer.eip = iret_emul_frame->iret.ip;
		ctrlxfer.esp = iret_emul_frame->iret.sp;
	    }
	    tag = preemption_reply_tag(cxfer);
	    L4_SetCtrlXferMask(&ctrlxfer, 0x3ff, CTRLXFER_FPU_ZERO);

	}

    static const L4_MsgTag_t yield_tag()
	{ return (L4_MsgTag_t) { X: { 2, CTRLXFER_SIZE, 0, msg_label_preemption_yield} } ;}

    void load_yield_msg(L4_ThreadId_t dest) 
	{ 
	    tag = yield_tag();
	    ctrlxfer.eax = dest.raw;
	    L4_SetCtrlXferMask(&ctrlxfer, 0x3ff, CTRLXFER_FPU_ZERO);
	    L4_Accept(L4_UntypedWordsAcceptor);
	}

    void dump();

};



#endif /* !__L4KA__VMEXT__THREAD_H__ */
