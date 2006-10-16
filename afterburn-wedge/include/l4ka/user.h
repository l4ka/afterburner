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
#ifndef __L4KA__USER_H__
#define __L4KA__USER_H__

#include INC_ARCH(page.h)
#include INC_ARCH(types.h)
#include INC_WEDGE(vcpulocal.h)

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


class mr_save_t
{
public:
    union {
	L4_Word_t raw[13];
	struct {
	    L4_MsgTag_t tag;
	} envelope;
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
	    L4_Word_t addr;
	    L4_Word_t ip;
	} pfault_msg;
	struct {
	    L4_MsgTag_t tag;
	    L4_Word_t vector;
	} vector_msg;
    };
    
    L4_Word_t get_pfault_ip() { return pfault_msg.ip; }
    L4_Word_t get_pfault_addr() { return pfault_msg.addr; }
    L4_Word_t get_pfault_rwx() { return L4_Label(pfault_msg.tag) & 0x7; }
    void store_pfault_msg(L4_MsgTag_t tag) 
	{
	    ASSERT (L4_UntypedWords(tag) == 2);
	    L4_StoreMR( 0, &pfault_msg.tag.raw );
	    L4_StoreMR( 1, &pfault_msg.addr );
	    L4_StoreMR( 2, &pfault_msg.ip );
	}
    void load_pfault_msg(L4_MapItem_t map_item) 
	{
	    L4_Msg_t msg;
	    L4_MsgClear( &msg );
	    L4_MsgAppendMapItem( &msg, map_item );
	    L4_MsgLoad( &msg );
	}
    L4_Word_t get_exc_ip() { return exc_msg.eip }
    void store_exception_msg(L4_MsgTag_t tag) 
	{	
	    thread_info->envelope.tag = tag;
	    L4_StoreMRs( 1, L4_UntypedWords(tag), 
		    &thread_info->raw[1] );
	}

};

    
#endif /* !__L4KA__USER_H__ */
