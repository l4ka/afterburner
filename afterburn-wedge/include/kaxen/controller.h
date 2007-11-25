/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/kaxen/controller.h
 * Description:   Stuff to support communication with Xen's domain controller.
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
#ifndef __AFTERBURN_WEDGE__INCLUDE__KAXEN__CONTROLLER_H__
#define __AFTERBURN_WEDGE__INCLUDE__KAXEN__CONTROLLER_H__

#include INC_WEDGE(xen_hypervisor.h)
//TODO amd64
//#include INC_WEDGE(cpu.h)

#ifdef CONFIG_XEN_2_0

// NOTE: Xen's interface to the domain controller has a massive race condition,
// which is especially nasty since debug code may try to send a message while
// other code is sending a message.  Unfortunately we have to live with it.
// XenoLinux disables interrupts to avoid races, but that isn't really good
// enough either.  To make this a nice reentrant interface, one should add a
// bit to the control_msg_t that indicates who owns the field, and transfer
// ownership of the message via this bit, and use a cmpxchg on the indices.
// Currently ownership is transferred by incrementing the indices, which isn't
// reentrant.

// NOTE: Producer indices increment indefinitely as opposed to using
// modulo arithmetic.  They are masked to convert into a valid index.

class xen_controller_t
{
public:
    void init()
    {
	ctrl_if = (volatile control_if_t *)(word_t(&xen_shared_info) + 2048);
    }

    void process_async_event( xen_frame_t *frame );
    char console_destructive_read();

    void console_write( char ch )
    {
	control_msg_t msg;
	msg.type = CMSG_CONSOLE;
	msg.subtype = CMSG_CONSOLE_DATA;
	msg.length = 1;
	msg.msg[0] = ch;
	blocking_tx( &msg );
    }

    bool is_tx_full()
    {
	return (ctrl_if->tx_req_prod - ctrl_if->tx_resp_prod) == CONTROL_RING_SIZE;
    }

private:
    volatile control_if_t *ctrl_if;

    void blocking_tx( control_msg_t *msg );

    void notify_controller_dom()
    {
	evtchn_op_t op;
	op.cmd = EVTCHNOP_send;
	op.u.send.local_port = xen_start_info.domain_controller_evtchn;
	XEN_event_channel_op( &op );
    }
};

extern xen_controller_t xen_controller;

#else

class xen_controller_t
{
public:
    void init() {}
    void console_write( char ch ) {}
    char console_destructive_read() { return '6'; }
    //TODO amd64
    //void process_async_event( xen_frame_t *frame ) {}
};

extern xen_controller_t xen_controller;

#endif

#endif	/* __AFTERBURN_WEDGE__INCLUDE__KAXEN__CONTROLLER_H__ */
