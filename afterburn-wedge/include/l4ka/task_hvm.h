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

#ifndef __L4KA__TASK_HVM_H__
#define __L4KA__TASK_HVM_H__

#include INC_ARCH(page.h)
#include INC_ARCH(types.h)

extern word_t user_vaddr_end;

class task_info_t
{
public:
    void init();
};


thread_info_t *allocate_thread();
void delete_thread( thread_info_t *thread_info );

INLINE void setup_thread_faults(L4_ThreadId_t tid, bool on, bool real_mode) 
{ 
    L4_Msg_t ctrlxfer_msg;
    L4_Word64_t fault_id_mask; 
    //by default, always send GPREGS, non regs and exc info
    L4_Word_t default_fault_mask =
	    (L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_GPREGS_ID) | 
	     L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_NONREGS_ID) | 
	     L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_EXC_ID));
    L4_Word_t fault_mask;

    if (on)
    {
	L4_Clear(&ctrlxfer_msg);
	
	if (real_mode)
	{
	    // on real mode, also send cs, ss, tr, ldtr register state
	    default_fault_mask |= 
	    (L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_CSREGS_ID) | 
	     L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_SSREGS_ID));
	}
	
	fault_id_mask = ((1ULL << L4_CTRLXFER_HVM_FAULT(hvm_vmx_reason_max))-1) & ~0x1c3;
	fault_mask = default_fault_mask;
	L4_AppendFaultConfCtrlXferItems(&ctrlxfer_msg, fault_id_mask, fault_mask, true);

	//for io faults, also send ds/es info 
	fault_id_mask = L4_CTRLXFER_FAULT_ID_MASK(L4_CTRLXFER_HVM_FAULT(hvm_vmx_reason_io));
	fault_mask = default_fault_mask |
	    L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_DSREGS_ID) |
	    L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_ESREGS_ID);
	
	L4_AppendFaultConfCtrlXferItems(&ctrlxfer_msg, fault_id_mask, fault_mask, false);
	L4_Load(&ctrlxfer_msg);
	L4_ConfCtrlXferItems(tid);

    }    

}


#endif /* !__L4KA__TASK_HVM_H__ */
