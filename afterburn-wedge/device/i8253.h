/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/device/i8253.h
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
 * $Id: i8253.h,v 1.13 2005/12/27 09:13:37 joshua Exp $
 *
 ********************************************************************/
#ifndef __DEVICE__I8253_H__
#define __DEVICE__I8253_H__

#include INC_ARCH(bitops.h)
#include INC_ARCH(cycles.h)
#include INC_WEDGE(intmath.h)

struct i8253_control_t
{
    enum read_write_e {rw_latch=0, rw_lsb=1, rw_msb=2, rw_lsb_msb=3};
    enum mode_e {mode_event=0, mode_one_shot=1, mode_periodic=2, 
	mode_square_wave=3, mode_soft_strobe=4, mode_hard_strobe=5, 
	mode_periodic_again=6, mode_square_wave_again=7};

    union {
	u8_t raw;
	struct {
	    u8_t enable_bcd : 1;
	    u8_t mode : 3;
	    u8_t read_write_mode : 2;
	    u8_t counter_select : 2;
	} fields;
    } x;

    read_write_e get_read_write_mode() 
	{ return (read_write_e)x.fields.read_write_mode; }
    mode_e get_mode()
	{ return (mode_e)x.fields.mode; }
    bool is_bcd()
	{ return x.fields.enable_bcd; }
    bool is_periodic()
	{ return get_mode() == mode_periodic; }
    word_t which_counter()
	{ return x.fields.counter_select; }
};

struct i8253_counter_t
{
    i8253_control_t control;
    bool first_write;
    cycles_t start_cycle;
    u16_t counter;

    static const word_t clock_rate = 1193182; /* Hz */
    static const word_t amd_elan_clock_rate = 1189200; /* Hz */

    word_t get_usecs()
    { return muldiv32(counter, 1000000, clock_rate); }
    word_t get_remaining_count();
    word_t get_remaining_usecs()
	{ return get_remaining_count() * 1000000 / clock_rate; }
    word_t get_out();
};

class i8253_t
{
public:
    enum port_e { 
	ch0_port=0x40, ch1_port=0x41, ch2_port=0x42, mode_port = 0x43,
    };

    static const word_t irq = 0;
    static const word_t pit_counter = 0;
    static const word_t speaker_counter = 2;

    i8253_counter_t counters[4];
};


#endif /*  __DEVICE__I8253_H__ */
