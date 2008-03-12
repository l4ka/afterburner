/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/device/8250.cc
 * Description:   Serial port emulation of the legacy 8250.
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
 * $Id: 8250.cc,v 1.8 2005/06/28 07:34:42 joshua Exp $
 *
 ********************************************************************/

#include <console.h>
#include <debug.h>
#include <device/portio.h>
#include INC_ARCH(intlogic.h)
#include INC_WEDGE(iostream.h)

struct mcr_t {  // Modem Control Register
    union {
	u8_t raw;
	struct {
	    u8_t force_dtr : 1;
	    u8_t force_req_to_send : 1;
	    u8_t aux_out1 : 1;
	    u8_t aux_out2 : 1;
	    u8_t loopback : 1;
	    u8_t autoflow_enabled : 1;
	    u8_t reserved : 2;
	} fields;
    } x __attribute__((packed));

    bool is_loopback()
	{ return x.fields.loopback; }
};

struct msr_t {	// Modem Status Register
    union {
	u8_t raw;
	struct {
	    u8_t delta_clear_to_send : 1;
	    u8_t delta_data_set_ready : 1;
	    u8_t trailing_edge_ring_indicator : 1;
	    u8_t delta_data_carrier_detect : 1;
	    u8_t clear_to_send : 1;
	    u8_t data_set_ready : 1;
	    u8_t ring_indicator : 1;
	    u8_t carrier_detect : 1;
	} fields;
    } x __attribute__((packed));
};

struct ier_t {	// interrupt enable register
    union {
	u8_t raw;
	struct {
	    u8_t enable_rcv_data : 1;
	    u8_t enable_tx_empty : 1;
	    u8_t enable_rcv_line : 1;
	    u8_t enable_modem_status : 1;
	    u8_t enable_sleep_mode : 1;
	    u8_t enable_low_power : 1;
	    u8_t : 2;
	} fields;
    } x __attribute__((packed));
};

struct iir_t { // interrupt identification register
    enum int_status_e {modem_status=0, tx_empty=1, rcv_data=2, rcv_line=3};

    union {
	u8_t raw;
	struct {
	    u8_t no_interrupt_pending : 1;
	    u8_t interrupt_status : 2;
	    u8_t zero : 5;
	} fields;
    } x __attribute__((packed));

    word_t pending;

    bool set_interrupt_pending( int_status_e status )
	{ 
	    if( !x.fields.no_interrupt_pending ) {
		pending |= 1 << status;
		return false;
	    }
	    else {
		x.fields.interrupt_status = status;
		x.fields.no_interrupt_pending = 0; 
		return true;
	    }
	}

    bool next_interrupt_pending()
	{
	    if( pending ) {
		word_t bit = lsb( pending );
		bit_clear( bit, pending );
		x.fields.interrupt_status = bit;
		x.fields.no_interrupt_pending = 0;
		return true;
	    }
	    else {
		x.fields.no_interrupt_pending = 1;
		return false;
	    }
	}

    bool clear_interrupt_pending( int_status_e status )
	{
	    bit_clear( status, pending );
	    if( x.fields.interrupt_status == status )
		return next_interrupt_pending();
	}

};

struct lsr_t {	// Line Status Register
    union {
	u8_t raw;
	struct {
	    u8_t data_ready : 1;
	    u8_t overrun_error : 1;
	    u8_t parity_error : 1;
	    u8_t framing_error : 1;
	    u8_t break_interrupt : 1;
	    u8_t empty_tx_holding : 1;
	    u8_t empty_data_holding : 1;
	    u8_t rcv_fifo_error : 1;
	} fields;
    } x __attribute__((packed));
};

struct lcr_t {	// Line Control Register
    union {
	u8_t raw;
	struct {
	    u8_t word_length : 2;
	    u8_t stop_bit_length : 1;
	    u8_t parity_select : 3;
	    u8_t set_break_enable : 1;
	    u8_t dlab : 1;
	} fields;
    } x __attribute__((packed));

    bool is_dlab_enabled()
	{ return x.fields.dlab; }
};

class serial8250_t
{
public:
    enum port_bases_e {
       	com1_base=0x3f8, com2_base=0x2f8, com3_base=0x3e8, com4_base=0x2e8,
    };
    enum port_irqs_e {
       	com1_irq=4, com2_irq=3, com3_irq=4, com4_irq=3,
    };

    port_bases_e port;
    port_irqs_e irq;

    ier_t ier;
    iir_t iir;
    lcr_t lcr;
    lsr_t lsr;
    mcr_t mcr;
    msr_t msr;

    u8_t dlab_low;
    u8_t dlab_high;

    u8_t recv_buffer;

    void raise_tx_interrupt();
    void raise_rx_interrupt();
    void disable_tx_interrupt();
    void disable_rx_interrupt();

    serial8250_t();

    static inline u16_t base_port( u16_t port )
	{ return port & ~7U; }
    static inline word_t reg_addr( u16_t port )
	{ return port & 7; }
};

class serial_ports_t {
public:
    serial8250_t ports[4];

    hiostream_t con;
    DEBUG_STREAM con_driver;

    serial_ports_t();
};

serial8250_t::serial8250_t()
{
    iir.x.raw = 0;
    iir.pending = 0;

    ier.x.raw = 0;
    lcr.x.raw = 0;
    mcr.x.raw = 0;

    msr.x.raw = 0;
    msr.x.fields.carrier_detect = 1;
    msr.x.fields.clear_to_send = 1;

    lsr.x.raw = 0;
    lsr.x.fields.empty_tx_holding = 1;
    lsr.x.fields.empty_data_holding = 1;

    // 115200 bps
    dlab_low = 1;
    dlab_high = 0;

    recv_buffer = 0;
}

void serial8250_t::raise_rx_interrupt()
{
    if( !ier.x.fields.enable_rcv_data )
	return;

    if( iir.set_interrupt_pending(iir_t::rcv_data) )
	get_intlogic().raise_irq( irq );
}

void serial8250_t::disable_rx_interrupt()
{
    ier.x.fields.enable_rcv_data = 0;
    iir.clear_interrupt_pending( iir_t::rcv_data );
}

void serial8250_t::raise_tx_interrupt()
{
    if( !ier.x.fields.enable_tx_empty )
	return;

    if( iir.set_interrupt_pending(iir_t::tx_empty) )
	get_intlogic().raise_irq( irq );
}

void serial8250_t::disable_tx_interrupt()
{
    ier.x.fields.enable_tx_empty = 0;
    iir.clear_interrupt_pending( iir_t::tx_empty );
}

serial_ports_t::serial_ports_t()
{
    // Configure the serial port's console driver.
    con_driver.init();
    con.init( &con_driver );

    // Assign each serial port an irq and a base address.
    ports[0].irq  = serial8250_t::com1_irq;
    ports[0].port = serial8250_t::com1_base;
    ports[1].irq  = serial8250_t::com2_irq;
    ports[1].port = serial8250_t::com2_base;
    ports[2].irq  = serial8250_t::com3_irq;
    ports[2].port = serial8250_t::com3_base;
    ports[3].irq  = serial8250_t::com4_irq;
    ports[3].port = serial8250_t::com4_base;
}

// Statically allocate the serial ports.
serial_ports_t serial_ports;

void serial8250_receive_byte( u8_t byte )
{
    if( !serial_ports.ports[0].lsr.x.fields.data_ready )
    {
	serial_ports.ports[0].recv_buffer = byte;
	serial_ports.ports[0].lsr.x.fields.data_ready = 1;
	serial_ports.ports[0].raise_rx_interrupt();
    }
}

void serial8250_portio( u16_t port, u32_t & value, bool read )
{
    // Figure out the offset of the i/o port from the serial port's base addr.
    word_t addr = serial8250_t::reg_addr( port );

    // Choose the serial port, based on the i/o port address.
    word_t which;
    switch( serial8250_t::base_port(port) )
    {
	case serial8250_t::com1_base: which = 0; break;
	case serial8250_t::com2_base: which = 1; break;
	case serial8250_t::com3_base: which = 2; break;
	case serial8250_t::com4_base: which = 3; break;
	default: return;
    }
    serial8250_t &dev = serial_ports.ports[which];

    if( read ) {
	switch( addr ) {
	    case 0:
		if( dev.lcr.is_dlab_enabled() )
		    value = dev.dlab_low;
		else {
		    value = dev.recv_buffer;
		    dev.lsr.x.fields.data_ready = 0;
		}
		break;
	    case 1:
		if( dev.lcr.is_dlab_enabled() )
		    value = dev.dlab_high;
		else
		    value = dev.ier.x.raw;
		break;
	    case 2: 
	    	value = dev.iir.x.raw;
    		if( dev.iir.next_interrupt_pending() )
		    get_intlogic().raise_irq( dev.irq );
		break;
	    case 3: value = dev.lcr.x.raw; break;
	    case 4: value = dev.mcr.x.raw; break;
	    case 5: value = dev.lsr.x.raw; break;
	    case 6: value = dev.msr.x.raw; break;
	    case 7: break;
	}
	return;
    }

    switch( addr ) {
	case 0:
	    if( dev.lcr.is_dlab_enabled() )
		dev.dlab_low = value;
	    else 
	    {
		serial_ports.con << (char)value;
		dev.raise_tx_interrupt();
	    }
	    break;
	case 1:
	    if( dev.lcr.is_dlab_enabled() )
		dev.dlab_high = value;
	    else {
		ier_t old_ier = dev.ier;
		dev.ier.x.raw = value;
		if( dev.ier.x.fields.enable_tx_empty && !old_ier.x.fields.enable_tx_empty )
		    dev.raise_tx_interrupt();
		else if( !dev.ier.x.fields.enable_tx_empty && old_ier.x.fields.enable_tx_empty )
		    dev.disable_tx_interrupt();
		if( dev.ier.x.fields.enable_rcv_data && !old_ier.x.fields.enable_rcv_data )
		    dev.raise_rx_interrupt();
		else if (!dev.ier.x.fields.enable_rcv_data && old_ier.x.fields.enable_rcv_data )
		    dev.disable_rx_interrupt();
	    }
	    break;
	case 2: break; // fcr write, dropped
	case 3: dev.lcr.x.raw = value; break;
	case 4: dev.mcr.x.raw = value; break;
	case 5: break;
	case 6: break;
	case 7: break;
    }
}

