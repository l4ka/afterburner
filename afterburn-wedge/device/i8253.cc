/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/device/i8253.cc
 * Description:   Front-end support for legacy 8253 emulation.
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
 * $Id: i8253.cc,v 1.8 2005/12/16 09:25:11 joshua Exp $
 *
 ********************************************************************/

#include <device/i8253.h>
#include <device/portio.h>
#include INC_WEDGE(vcpulocal.h)
#include <console.h>
#include INC_WEDGE(backend.h)
#include INC_WEDGE(intmath.h)


/* Documentation: http://www.mega-tokyo.com/osfaq2/index.php/PIT
 * Only channel 0 is connected to the PIC, at IRQ 0.  Channel 2
 * is connected to the PC speaker.
 */

INLINE word_t cycles_to_usecs( cycles_t cycles )
{
    ASSERT(get_vcpu().cpu_hz >= 1000);
    return ((get_vcpu().cpu_hz < 1000000) ? 
	    ((1000 * cycles) / (get_vcpu().cpu_hz / 1000)) :
	    (cycles / (get_vcpu().cpu_hz / 1000000)));
}


i8253_t i8253;

word_t i8253_counter_t::get_remaining_count()
{
    word_t delta_cycles = (word_t)(get_cycles() - start_cycle);
    word_t delta_usecs = cycles_to_usecs( delta_cycles );
    word_t delta_count = delta_usecs * clock_rate / 1000000;

    if( control.is_periodic() )
	return delta_count % counter;
    else if( delta_count > counter )
	return 0;
    else
	return counter - delta_count;
}

void i8253_portio( u16_t port, u32_t & value, bool read )
{
    dprintf(debug_pit, "i8253 portio %c %x val %x\n", 
	    (read ? 'r' : 'w'), port, value);
    
    if( port == i8253_t::mode_port )
    {
	/* Control operations */
	if( read ) {
	    printf( "Unsupported i8253 control port read.\n");
	    return;
	}

	i8253_control_t control = {x: {raw: value}};
#if defined(CONFIG_DEVICE_PASSTHRU_PCSPEAKER)
	/* Counter 2 is used for PC Speaker */
	if(control.which_counter() == 2) {
	    __asm__ __volatile__ ("outb %b0, %1\n" : : "a"(value), "dN"(port) );
	    return;
	}
#endif
	i8253.counters[ control.which_counter() ].control = control;
	i8253.counters[ control.which_counter() ].first_write = true;

	dprintf(debug_pit, "i8253 - %x, mode %x, counter %x, BCD %d\n",
		control.which_counter(), control.get_mode(),
		i8253.counters[ control.which_counter()].counter,
		control.is_bcd());;
	return;
    }

    /* 
     * Counter operations. 
     */

    unsigned which = port - i8253_t::ch0_port;
    if( which > 2 ) {
	printf( "Error: invalid i8253 port: %x ", port);
	return;
    }
#if defined(CONFIG_DEVICE_PASSTHRU_PCSPEAKER)
    /* PC Speaker */
    if(which == 2) {
	u32_t tmp;
	if(read) {
	    __asm__ __volatile__ ("inb %1, %b0\n" : "=a"(tmp) : "dN"(port) );
	    value = tmp;
	}
	else {
	    __asm__ __volatile__ ("outb %b0, %1\n" : : "a"(value), "dN"(port) );
	}
	return;
    }

#endif
    i8253_counter_t & counter = i8253.counters[which];

    if( read ) {
	value = counter.get_remaining_count();
	dprintf(debug_pit, "i8253 - %d, counter read %x, remaining %x\n",
		    which, counter.counter, value);
	return;
    }

    switch( counter.control.get_read_write_mode() )
    {
    case i8253_control_t::rw_latch :
	break;
    case i8253_control_t::rw_lsb :
	// In general, writing the lsb pauses the count down.
	counter.counter = (counter.counter & 0xff00) | (0xff & value);
	break;
    case i8253_control_t::rw_msb :
	// In general, writing the msb resumes the count down.
	counter.counter = (counter.counter & 0x00ff) | ((0xff & value) << 8);
	counter.start_cycle = get_cycles();
	dprintf(debug_pit, "i8253 - %d, new counter %x, microseconds: %x\n",
		    which, counter.counter, counter.get_usecs());
	break;
    case i8253_control_t::rw_lsb_msb :
	if( counter.first_write )
	    counter.counter = (counter.counter & 0xff00) | (0xff & value);
	else {
	    counter.counter = (counter.counter & 0x00ff) | ((0xff & value) << 8);
	    counter.start_cycle = get_cycles();
	    dprintf(debug_pit, "i8253 - %d, new counter %x, microseconds: %x\n",
			which, counter.counter, counter.get_usecs());
	}
	counter.first_write = !counter.first_write;
	break;
    }

}

/***************************************************************************/

class legacy_0x61_t
{
public:
    union {
	u8_t raw;
	struct {
	    u8_t timer2_enable : 1;
	    u8_t speaker_enable : 1;
	    u8_t pci_serr_nmi_disable : 1;
	    u8_t iochk_nmi_disable : 1;
	    u8_t refresh_cycle_toggle : 1;
	    u8_t timer2_out_status : 1;
	    u8_t iochk_nmi_status : 1;
	    u8_t pci_serr_nmi_status : 1;
	} fields;
    } x;
};

static legacy_0x61_t register_0x61;

void legacy_0x61( u16_t port, u32_t &value, bool read )
{
#if defined(CONFIG_DEVICE_PASSTHRU_PCSPEAKER)
    u32_t tmp;
    if(read) {
	__asm__ __volatile__ ("inb %1, %b0\n" : "=a"(tmp) : "dN"(port) );
	value = tmp;
    }
    else {
	__asm__ __volatile__ ("outb %b0, %1\n" : : "a"(value), "dN"(port) );
    }
    return;
#else
    if( !read ) {
	register_0x61.x.raw = value;
	return;
    }

    legacy_0x61_t tmp = register_0x61;

    if( tmp.x.fields.timer2_enable )
    {
	// A guest OS may be polling on the timer2 status, since timer2
	// doesn't raise an IRQ.
	tmp.x.fields.timer2_out_status = i8253.counters[ i8253_t::speaker_counter ].get_remaining_count() > 0;
    }

    value = tmp.x.raw;
#endif
}


word_t timer_remaining_usec( void )
{
    return i8253.counters[ i8253_t::pit_counter ].get_remaining_usecs();
}

