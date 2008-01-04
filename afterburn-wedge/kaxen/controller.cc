/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/kaxen/controller.cc
 * Description:   Code for interacting with the Xen domain controller.
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

#include INC_WEDGE(xen_hypervisor.h)
#include INC_WEDGE(controller.h)
#include <debug.h>

#include <memory.h>

xen_controller_t xen_controller;

void xen_controller_t::blocking_tx( control_msg_t *msg )
    // Send a control message, waiting until it is possible to do so.
{
    // Wait for a free entry in the ring.
    word_t next;
    while( 1 ) {
	next = ctrl_if->tx_req_prod;
	if( is_tx_full() )
	    XEN_yield();
	else
	    break;
    }

    // Install the control message into the tx ring.
    memcpy( (void *)&ctrl_if->tx_ring[MASK_CONTROL_IDX(next)], 
	    msg, sizeof(*msg) );
    ctrl_if->tx_req_prod++;
    notify_controller_dom();
}

char xen_controller_t::console_destructive_read()
    // This function searches for console input.  Since Xen is silly,
    // network and disk messages share the same producer/consumer ring
    // with the console.  This function is destructive, and discards
    // all disk and network messages that it finds, so that it can
    // make room for more console messages.  Furthermore, this
    // function discards extra console input; it accepts only a single
    // character at a time.
{
    while( 1 ) {
	// Wait until a control message arrives.
	while( ctrl_if->rx_resp_prod == ctrl_if->rx_req_prod )
	    XEN_yield();

	// Extract the control message.
	control_msg_t msg;
	memcpy( &msg, (void *)&ctrl_if->rx_ring[MASK_CONTROL_IDX(ctrl_if->rx_resp_prod)], sizeof(msg) );
	ctrl_if->rx_resp_prod++;

	// See if the control message has console character data.
	if( (msg.type == CMSG_CONSOLE) && (msg.subtype == CMSG_CONSOLE_DATA)
		&& (msg.length > 0) )
	    return msg.msg[0];
    }
}

void xen_controller_t::process_async_event( xen_frame_t *frame )
    // WARNING: It is dangerous to manipulate wedge data structures in this
    // function, since it is an asynchronous callback.
{
    extern void serial8250_receive_byte( u8_t byte );

    while( ctrl_if->rx_resp_prod != ctrl_if->rx_req_prod )
    {
	// Extract the control message.
	control_msg_t msg;
	memcpy( &msg, (void *)&ctrl_if->rx_ring[MASK_CONTROL_IDX(ctrl_if->rx_resp_prod)], sizeof(msg) );
	ctrl_if->rx_resp_prod++;

	// Handle or discard the message.
	if( (msg.type == CMSG_CONSOLE) && (msg.subtype == CMSG_CONSOLE_DATA)
		&& (msg.length > 0) )
	{
	    char key = msg.msg[0];
#if defined(CONFIG_DEBUGGER)
	    if( CONFIG_DEBUGGER_BREAKIN_KEY == key )
		debugger_enter( frame );
	    else
#endif
	    {
		serial8250_receive_byte( (u8_t)key );
	    }
	}
    }
}
