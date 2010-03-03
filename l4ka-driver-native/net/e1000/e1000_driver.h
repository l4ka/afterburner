/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     net/e1000/e1000_driver.h
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
#ifndef __NET__E1000__E1000_DRIVER_H__
#define __NET__E1000__E1000_DRIVER_H__

#include <pci/pci_config.h>
#include <net/e1000/e1000.h>
#include <net/e1000/e1000_hw.h>
#include <net/net.h>
#include <common/ia32/page.h>

#if defined(CONFIG_SMP)
#include <common/ia32/bitops.h>
#define SMP_OP(a)	a
#else
#define SMP_OP(a)
#endif

class E1000TxDesc : public e1000_tx_desc
{
public:
    E1000TxDesc() { clear(); }
    void clear()
    {
	buffer_low = 0;
	buffer_high = 0;
	lower.data = 0;
	upper.data = 0;
    }
};

class E1000RxDesc : public e1000_rx_desc
{
public:
    E1000RxDesc() { clear(); }

    void clear()
    {
	buffer_low = 0;
	buffer_high = 0;
	length = 0;
	csum = 0;
	status = 0;
	errors = 0;
	special = 0;
    }

    void release_to_device() { status = 0; }
};

#define PROMISCUOUS_LAN_ADDR	1
#define PROMISCUOUS_IFACE	2
#define PROMISCUOUS_MCAST	4

class E1000Driver
{
public:
    // Lookup functions
    L4_ThreadId_t get_irq_tid() { return this->irq_tid; }
    void get_mac_addr( u8_t mac_addr[NODE_ADDRESS_SIZE] )
    { 
	for( unsigned i = 0; i < NODE_ADDRESS_SIZE; i++ )
	    mac_addr[i] = this->e1000_hw.mac_addr[i];
    }

public:
    PciDeviceConfig pci_config;
    int cpu;

protected:
    struct e1000_hw e1000_hw;
    struct e1000_phy_info phy_info;
    int which;
    u32_t part_num;
    L4_ThreadId_t irq_tid;
    NetIface *iface_list;

    u8_t promiscuous; // What is our promiscuous mode?
    u8_t last_csum_start;
    u8_t last_csum_offset;
    u16_t iface_cnt;
    word_t lan_addr_bitmap;

    SMP_OP(volatile u8_t cpu_lock);
    SMP_OP(volatile bool event_pending);
    SMP_OP(volatile bool irq_pending);
    SMP_OP(volatile word_t active);

protected:
    // Must be a multiple of 8
    static const word_t rx_count = 128;
    static const word_t tx_count = 2*PAGE_SIZE/sizeof(E1000TxDesc);

    u16_t mtu;
    u16_t tx_int_delay; // transmit interrupt delay
    u16_t rx_int_delay;
    u16_t tx_abs_int_delay; // transmit absolute interrupt delay
    u16_t rx_abs_int_delay;
    u32_t int_throttle_rate;  // interrupts/sec
    bool rx_csum;	// enables checksum offload on 82543 NICs
    u32_t txd_cmd;
    NetServerShared *server_shared;
    NetIface *tx_shadow[ tx_count ];
    NetRing<tx_count, E1000TxDesc> tx_ring ALIGNED(sizeof(E1000TxDesc));
    NetRing<rx_count, E1000RxDesc> rx_ring ALIGNED(sizeof(E1000RxDesc));

public:
    bool init( int which, PciDeviceConfig *pci_config );
    void reset();
    bool open();
    bool close();

    int get_index() { return this->which; }

    void irq_handler( L4_ThreadId_t active_tid );
    void send_handler( L4_ThreadId_t active_tid );

    void link_status_report();

    void remove_iface( NetIface *iface );
    void add_iface( NetIface *iface )
    {
	if( !iface->is_active() )
	    // The iface isn't completely configured ...
	    return;

	// Add to the iface linked list.
	iface->next = this->iface_list;
	this->iface_list = iface;
	this->iface_cnt++;

	// Add the iface's lan addr to the card's scanner.
	this->enable_lan_addr( iface );
    }

    void run_send_queue();

    bool is_promiscuous() { return this->promiscuous != 0; }

    bool lock_device()
    {
#if defined(CONFIG_SMP)
	this->event_pending = true;
	if( this->cpu_lock )
	    return false;
	if( test_and_set_bit(0, &this->cpu_lock) )
	    return false;
	ASSERT( this->active == 0 );
	this->active++;
	this->event_pending = false;
#endif
	return true;
    }
    void spin_lock_device()
    {
	SMP_OP( while(test_and_set_bit(0, &this->cpu_lock)) )
	    ;
    }
    bool unlock_device()
    {
#if defined(CONFIG_SMP)
	ASSERT( this->active == 1 );
	this->active--;
	clear_bit( 0, &this->cpu_lock );
	return this->event_pending;
#endif
	return false;
    }

protected:
    bool map_device();
    void configure_tx();
    void configure_rx();
    void setup_rctl();
    bool setup_tx_resources();
    bool setup_rx_resources();
    void irq_enable();
    void irq_disable();
    u16_t clean_tx_ring();

    void enable_rx_copy_buffers();
    u16_t clean_copy_rx();

    bool tx_inject_csum( NetClientDesc *client_desc );
    int tx_packet( NetIface *iface, NetClientDesc *client_desc );

    void enable_promiscuous( u32_t flag );
    void disable_promiscuous( u32_t flag );
    void enable_lan_addr( NetIface *iface );
    void enable_promiscuous_lan_addr()
    {
	this->enable_promiscuous( E1000_RCTL_UPE | E1000_RCTL_MPE );
	this->promiscuous |= PROMISCUOUS_LAN_ADDR;
    }

    void deliver_pending_client_msgs( L4_ThreadId_t active_tid )
    {
	for( NetIface *iface = this->iface_list; iface; iface = iface->next )
	    iface->deliver_pending_msg( active_tid );
    }
    void deliver_pending_client_pkts()
    {
	for( NetIface *iface = this->iface_list; iface; iface = iface->next ) {
	    // We try some packet batching.  2 gives good results.
	    // 3 diminishes netperf-send throughput.
	    if( iface->rx_pending() > 2 || tx_ring.is_empty() )
		iface->flush_pkt_queue();
	}
    }

    unsigned int max_ring_descriptors()
    {
	return (this->e1000_hw.mac_type < e1000_82544) ? 256 : 4096;
    }

    unsigned int min_ring_descriptors()  { return 80; }

    // Ring descriptors must be a multiple of 8.
    unsigned int mask_ring_descriptors() { return ~UL(7); }
};

#if defined(NET_STATS)
extern inline NetStats *get_stats( E1000Driver *driver )
{
    return &get_server_status(driver->get_index())->stats[ L4_ProcessorNo() ];
}
#endif

#endif	/* __NET__E1000__E1000_DRIVER_H__ */
