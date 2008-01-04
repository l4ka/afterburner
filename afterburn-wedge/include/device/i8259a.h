/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/device/i8259a.h
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
 * $Id: i8259a.h,v 1.13 2005/12/27 09:13:37 joshua Exp $
 *
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__DEVICE__I8259A_H__
#define __AFTERBURN_WEDGE__INCLUDE__DEVICE__I8259A_H__

#include INC_ARCH(bitops.h)
#include <debug.h>

class i8259a_icw1_t
{
private:
    volatile union {
	u8_t raw;
	struct {
	    u8_t icw4 : 1;
	    u8_t ms   : 1;
	    u8_t      : 1;
	    u8_t trig : 1;
	    u8_t      : 1;	// Always 1
	    u8_t      : 3;
	} fields;
    } x;

public:
    void set_raw( u8_t new_val )
	{ x.raw = (new_val & 0xb) | 0x10; }
    u8_t get_raw()
	{ return x.raw; }

    bool master_only()
	{ return 1 == x.fields.ms; }
    bool level_triggered()
	{ return 1 == x.fields.trig; }
    bool icw4_enabled()
	{ return 1 == x.fields.icw4; }
};

class i8259a_icw2_t {
private:
    volatile u8_t raw;

public:
    u8_t get_raw()
	{ return raw; }
    void set_raw( u8_t new_val )
	{ raw = new_val & 0xf8; }

    u8_t get_idt_offset()
	{ return raw; }
};

class i8259a_icw3_t {
private:
    volatile u8_t raw;

public:
    void set_raw( u8_t new_val )
	{ raw = new_val; }
    u8_t get_raw()
	{ return raw; }

    u8_t slave_get_master_irq() 
	// Return the IRQ on the master that this slave is connected to.
	{ return raw & 0xc; }
};

class i8259a_icw4_t {
private:
    volatile union {
	u8_t raw;
	struct {
	    u8_t mode : 1;
	    u8_t aeoi : 1;
	    u8_t ms   : 1;
	    u8_t buf  : 1;
	    u8_t sfnm : 1;
	    u8_t      : 3;
	} fields;
    } x;

public:
    void set_raw( u8_t new_val )
	{ x.raw = new_val & 0x1f; }
    u8_t get_raw()
	{ return x.raw; }

    bool is_master()
	{ return 1 == x.fields.ms; }
    bool is_slave()
	{ return 0 == x.fields.ms; }
    bool is_auto_eoi()
	{ return 1 == x.fields.aeoi; }
    bool is_buffered_mode()
	{ return 1 == x.fields.buf; }
    bool is_fully_nested_mode()
	{ return 1 == x.fields.sfnm; }
};

class i8259a_ocw_t {
public:
    enum ocw2_op_e {
	clear_rotate_on_aeoi=0, non_specific_eoi=1, nop=2,
	specific_eoi=3, set_rotate_on_aeoi=4, rotate_on_non_specific_eoi=5, 
	set_priority=6, rotate_on_specific_eoi=7
    };

    union {
	u8_t raw;
	struct {
	    u8_t level : 3;
	    u8_t d3 : 1;
	    u8_t d4 : 1;
	    u8_t op : 3;
	} ocw2;
	struct {
	    u8_t ris  : 1;
	    u8_t rr   : 1;
	    u8_t p    : 1;
	    u8_t d3   : 1;
	    u8_t d4   : 1;
	    u8_t smm  : 1;
	    u8_t esmm : 1;
	    u8_t      : 1;
	} ocw3;
    } x;

public:
    bool is_icw1()
	{ return 1 == x.ocw2.d4; }
    bool is_ocw2()
	{ return 0 == x.ocw2.d3; }
    bool is_ocw3()
	{ return 1 == x.ocw2.d3; }
    bool is_specific_eoi()
	{ return get_op() == specific_eoi; }
    bool is_non_specific_eoi()
	{ return get_op() == non_specific_eoi; }

    bool is_read_request()
	{ return x.ocw3.rr; }
    bool is_poll_mode()
	{ return x.ocw3.p; }
    bool is_read_isr()
	{ return x.ocw3.ris == 1; }

    u8_t get_level()
	{ return x.ocw2.level; }
    ocw2_op_e get_op()
	{ return (ocw2_op_e)x.ocw2.op; }
};

#ifdef CONFIG_WEDGE_XEN
extern void backend_eoi(void);
#endif

class i8259a_t
{
public:
    // Port a --> i8259a's A0=0
    // Port b --> i8259a's A0=1
    enum port_e 
	{master_port_a = 0x20, master_port_b = 0x21,
	 slave_port_a  = 0xa0, slave_port_b  = 0xa1};
    enum mode_e 
	{ocw_mode=0, icw2_mode=1, icw3_mode=2, icw4_mode=3, max_mode=4};

public:
    i8259a_icw1_t icw1;
    i8259a_icw2_t icw2;
    i8259a_icw3_t icw3;
    i8259a_icw4_t icw4;

    volatile mode_e mode;

    volatile word_t irq_in_service;	// ISR - the CPU services this IRQ
    volatile word_t irq_request;	// IRR - a device requires servicing
    volatile word_t irq_mask;		// IMR - masks the IRR.
					// When an ISR is set, its IRR is reset.
					// In AEOI mode, the ISR bit automatically
					// and immediately resets.

    

    i8259a_ocw_t ocw_read;

    bool pending_vector( word_t & vector, word_t & irq, const word_t irq_base);

    word_t eoi( word_t level )
	{
		irq_in_service &= ~(1 << level); 
		return level;
	}

    word_t eoi()
	{
	    if( irq_in_service )
		return (eoi( lsb(irq_in_service)));
	    return 0;
	}

    void raise_irq( word_t irq, const word_t irq_base);

    void reset_mode()
	{ mode = icw2_mode; }
    void next_mode()
	{ 
	    mode = (mode_e)(((u32_t)mode + 1) % (u32_t)max_mode); 
	    if( (mode == icw3_mode) && icw1.master_only() )
		mode = (mode_e)(((u32_t)mode + 1) % (u32_t)max_mode); 
	    if( (mode == icw4_mode) && !icw1.icw4_enabled() )
		mode = (mode_e)(((u32_t)mode + 1) % (u32_t)max_mode); 
	}

    u8_t port_a_read();
    u8_t port_b_read();

    void port_a_write( u8_t value );
    void port_b_write( u8_t value, u8_t irq_base );

    i8259a_t()
	{
	    mode = ocw_mode; irq_mask = 0xff; irq_in_service = 0; 
	    ocw_read.x.ocw3.rr = 1; ocw_read.x.ocw3.ris = 0;
	}
};

#endif	/* __AFTERBURN_WEDGE__INCLUDE__DEVICE__I8259A_H__ */
