/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4-common/monitor.cc
 * Description:   The monitor thread, which handles wedge faults
 *                (primarily page faults).
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
 ********************************************************************/

#include <l4/thread.h>
#include <l4/ipc.h>

#include <l4-common/monitor.h>
#include <l4-common/message.h>

#include INC_ARCH(page.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(tlocal.h)
#include INC_WEDGE(backend.h)

static const bool debug_pfault=0;

static bool handle_pagefault( L4_MsgTag_t tag, L4_ThreadId_t tid )
{
    word_t fault_addr, ip, map_addr;
    L4_MapItem_t map_item;
    word_t rwx, page_bits;

    if( L4_UntypedWords(tag) != 2 ) {
	con << "Invalid page fault message from TID " << tid << '\n';
	return false;
    }

    rwx = L4_Label(tag) & 0x7;
    L4_StoreMR( 1, (L4_Word_t *)&fault_addr );
    L4_StoreMR( 2, (L4_Word_t *)&ip );

    if( debug_pfault )
	con << "pfault, addr: " << (void *)fault_addr 
	    << ", ip: " << (void *)ip << ", rwx: " << (void *)rwx
     	    << ", TID: " << tid << '\n';

    backend_handle_pagefault( tid, fault_addr, ip, map_addr, page_bits, rwx );

    if( (map_addr & ~((1UL << page_bits)-1)) == (fault_addr & ~((1UL << page_bits) -1)))
	map_item = L4_MapItem( L4_Nilpage, 0 );
    else
    {
	map_item = L4_MapItem( 
	      	L4_FpageAddRights(L4_FpageLog2(map_addr, page_bits), rwx),
	       	fault_addr );
    }

    L4_Msg_t msg;
    L4_MsgClear( &msg );
    L4_MsgAppendMapItem( &msg, map_item );
    L4_MsgLoad( &msg );
    return true;
}

void monitor_loop( vcpu_t & vcpu )
{
    con << "Entering monitor loop, TID " << L4_Myself() << '\n';

    L4_ThreadId_t tid = L4_nilthread;
    for (;;) {
	L4_MsgTag_t tag = L4_ReplyWait( tid, &tid );

	if( L4_IpcFailed(tag) ) {
	    tid = L4_nilthread;
	    continue;
	}

	switch( L4_Label(tag) >> 4 )
	{
	case msg_label_pfault:
	    if( !handle_pagefault(tag, tid) )
		tid = L4_nilthread;
	    break;

	case msg_label_except:
	    L4_Word_t ip;
	    L4_StoreMR( 1, &ip );
	    con << "Unhandled kernel exception, ip " << (void *)ip << '\n';
	    panic();

	default:
	    con << "Unhandled message " << (void *)tag.raw
		<< " from TID " << tid << '\n';
	    L4_KDB_Enter("monitor: unhandled message");
	}

    }
}

