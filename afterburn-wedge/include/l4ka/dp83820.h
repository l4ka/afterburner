/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/l4ka/dp83820.h
 * Description:   The L4Ka support for the DP83820 device model.
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
 * $Id: dp83820.h,v 1.5 2006/09/21 13:48:29 joshua Exp $
 *
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__L4KA__DP83820_H__
#define __AFTERBURN_WEDGE__INCLUDE__L4KA__DP83820_H__

#if defined(CONFIG_DEVICE_DP83820)

#include <l4/types.h>
#include INC_WEDGE(l4thread.h)

#include "L4VMnet_idl_client.h"

#if IDL4_HEADER_REVISION < 20030403
# error "Your version of IDL4 is too old.  Please upgrade to the latest."
#endif

struct dp83820_desc_t;

struct l4ka_net_ring_t {
    u16_t cnt;
    u16_t start_free;
    u16_t start_dirty;

    u16_t free()
	{ return (start_dirty + cnt - (start_free+1)) % cnt; }
    u16_t dirty()
	{ return cnt - 1 - free(); }
};

struct l4ka_net_rcv_group_t {
    static const word_t nr_desc = IVMnet_rcv_buffer_cnt + 1;

    int group_no;

    l4ka_net_ring_t ring;
    dp83820_desc_t *desc_ring[ nr_desc ];

    volatile bool waiting;

    L4_ThreadId_t dev_tid;
    
    l4thread_t *l4thread;
    u8_t thread_stack[KB(4)];
};

struct dp83820_backend_t {
    static const word_t nr_rcv_group = 1;

    L4_ThreadId_t server_tid;
    L4_Fpage_t shared_window;
    IVMnet_handle_t ivmnet_handle;

    IVMnet_client_shared_t *client_shared;
    IVMnet_server_shared_t *server_status;

    word_t group_count;
    volatile word_t idle_count;
    l4ka_net_rcv_group_t rcv_group[ nr_rcv_group ];

public:
    bool is_window_pfault( word_t fault_addr ) {
	return (fault_addr >= L4_Address(shared_window)) &&
	    (fault_addr < (L4_Address(shared_window) + L4_Size(shared_window)));
    }

    void handle_pfault( word_t fault_addr );
};

#endif	/* CONFIG_DEVICE_DP83820 */

#endif	/* __AFTERBURN_WEDGE__INCLUDE__L4KA__DP83820_H__ */
