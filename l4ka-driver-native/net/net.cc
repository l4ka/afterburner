/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     net/net.cc
 * Description:   
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
 * $Id$
 *                
 ********************************************************************/

#include <net/net.h>
#include <net/e1000/e1000_driver.h>

#if !defined(cfg_optimize)
int debug_net = 2;
#endif

#if defined(CONFIG_PACKET_COPY)
NetRxPacketPool pkt_pool;
#endif

void net_rotate_debug_mode()
{
#if !defined(cfg_optimize)
    debug_net = (debug_net + 1) % 10;
    printf( "lxserv::net: debug level %d\n", debug_net );
#endif
}

INLINE void prefetch( const void *x )
{
#if defined(__i386__)
    __asm__ __volatile__ ("prefetchnta (%0)" : : "r"(x) );
#else
# warning "Please provide a prefetch implementation for your architecture!"
#endif
}

template <const word_t ring_slots, typename T>
void NetRing<ring_slots, T>::init()
{
    free = dirty = 0;

    for( word_t i = 0; i < slots; i++ )
	ring[i] = NULL;
}

void NetIface::init( net_handle_t handle, E1000Driver *driver, NetClientShared *client_shared )
{
    this->driver = driver;
    this->shared = client_shared;

    this->active = false;
    this->promiscuous = false;
    this->pending_irq = false;

    this->rx_ring.init();

    client_shared->x.client_main_tid = L4_nilthread;
    client_shared->x.client_irq_tid = L4_nilthread;
    client_shared->x.client_irq_pending = false;
    client_shared->x.receiver_cnt = 0;
    for( int i = 0; i < IVMnet_max_receiver_cnt; i++ )
	client_shared->x.receiver_tids[i] = L4_nilthread;

    client_shared->x.server_irq = 0;
    client_shared->x.server_irq_tid = L4_nilthread;
    client_shared->x.server_main_tid = L4_nilthread;
}

bool NetIface::send_irq( L4_ThreadId_t active_tid )
{
    this->shared->x.client_irq_pending = true;
    
    L4_MsgTag_t tag, result_tag;
    tag.raw = 0;
    tag.X.u = 1;
    tag.X.label = 0x100;

    if( active_tid == shared->x.client_irq_tid ) {
	// We are about to perform an IPC reply to this thread.  If we send
	// it an IRQ IPC, we'll confuse it.
	return true;
    }

    L4_Set_MsgTag( tag );
    L4_LoadMR( 1, shared->x.client_irq );
    result_tag = L4_Send( shared->x.client_irq_tid );

    if( L4_IpcFailed(result_tag) ) {
	this->shared->x.client_irq_pending = false;
	printf( "send_irq() failed\n" );
	return false;
    }

    return true;
}

void NetIface::activate()
{
    ASSERT( this->shared );
    this->active = true;
    this->driver->add_iface( this );

    // Send IRQ message to client, so that it knows everything is enabled.
#if 0
    this->shared->irq.bits.link_status_change = 1;
    this->schedule_irq();
    this->deliver_pending_msg( L4_nilthread );
#endif
}

void NetIface::deactivate()
{
    nprintf( 2, "l4net: deactivate interface\n" );

    // First remove from the driver!
    this->driver->remove_iface( this );

    this->active = false;
}

void NetIface::flush_pkt_queue()
{
    xmit_pkts_to_client( 0 );
}

bool NetIface::xmit_pkts_to_client( int receiver )
{
    L4_ThreadId_t receiver_tid = this->shared->x.receiver_tids[ receiver ];
    if( L4_IsNilThread(receiver_tid) )
	return false;

    if( this->rx_ring.is_empty() )
	return true;

    // Install string items for the message
    L4_StringItem_t str_item;
    L4_Word_t transferred_bytes = 0;
    L4_Word_t string_items = 0;
    L4_Word_t idx = this->rx_ring.dirty;
    while( string_items < IVMnet_rcv_buffer_cnt )
    {
	NetRxPacket *pkt = this->rx_ring.ring[ idx ];
	ASSERT( pkt );

	if( string_items > 0 )
	{
	    // Store the string item from the last iteration.  We delay
	    // by one iteration to update the C bit.
	    str_item.X.C = 1;
	    L4_LoadMR( 2*string_items-1, str_item.raw[0] );
	    L4_LoadMR( 2*string_items,   str_item.raw[1] );
	}

	str_item = L4_StringItem( pkt->pkt_len, pkt->raw );
	transferred_bytes += pkt->pkt_len;

	string_items++;
	idx = (idx + 1) % this->rx_ring.slots;
	if( idx == this->rx_ring.free )
	    break; // No more packets.
    }
    nprintf( 4, "l4net: %lu inbound packets, %lu bytes to transfer.\n",
	    string_items, transferred_bytes );

    if( string_items == 0 )
	return true;

    // Write the last string item.
    L4_LoadMR( 2*string_items-1, str_item.raw[0] );
    L4_LoadMR( 2*string_items,   str_item.raw[1] );

    // Initialize the IPC message tag.
    L4_MsgTag_t tag = {raw: 0};
    tag.X.t = 2*string_items;
    L4_Set_MsgTag( tag );

    this->shared->x.receiver_tids[receiver] = L4_nilthread;

    // Deliver the IPC.
    L4_Set_XferTimeouts( L4_Timeouts(L4_Never, L4_Never) );
    tag = L4_Reply( receiver_tid );
    if( EXPECT_FALSE(L4_IpcFailed(tag)) )
    {
	L4_Word_t err = L4_ErrorCode();
	if( ((err >> 1) & 7) <= 3 ) {
	    // Send-phase error.
	    this->shared->x.receiver_tids[receiver] = receiver_tid;
	    transferred_bytes = 0;
	}
	else {
	    // Message overflow.
	    transferred_bytes = err >> 4;
	}
    }

    nprintf( 4, "l4net: %lu bytes transferred.\n", transferred_bytes );

    // Commit the mappings.  We may have had partial transfer.  We resend
    // any packets that had partial transfer.
    for( idx = 0; (idx < string_items) && transferred_bytes; idx++ )
    {
	NetRxPacket *pkt = this->rx_ring.ring[ this->rx_ring.dirty ];
	if( transferred_bytes >= pkt->pkt_len )
	{
	    transferred_bytes -= pkt->pkt_len;
	    this->rx_ring.ring[ this->rx_ring.dirty ] = NULL;
	    this->rx_ring.inc_dirty();

	    pkt->dec();
	    if( pkt->is_free() )
		get_pkt_pool()->push( pkt );
	}
	else
	    transferred_bytes = 0;
    }

    return true;
}

