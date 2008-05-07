/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/device/mc146818rtc.cc
 * Description:   Front-end support for the real-time-clock emulation,
 *                on the legacy x86 platform.
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
 * $Id: mc146818rtc.cc,v 1.8 2005/11/15 00:55:42 uhlig Exp $
 *
 ********************************************************************/

#include <device/rtc.h>
#include <device/portio.h>
#include <aftertime.h>
#include <console.h>
#include <debug.h>
#include INC_WEDGE(backend.h)


#if  defined(CONFIG_DEVICE_PASSTHRU) && !defined(CONFIG_WEDGE_L4KA)

void mc146818rtc_portio( u16_t port, u32_t & value, bool read )
{
    word_t tmp;
    if( read ) {
	__asm__ __volatile__ ("inb %1, %b0\n" : "=a"(tmp) : "dN"(port) );
	value = tmp;
    }
    else {
	tmp = value;
	__asm__ __volatile__ ("outb %b1, %0\n" : : "dn"(port), "a"(tmp) );
    }
}

#else

/*
 * http://bochs.sourceforge.net/techspec/CMOS-reference.txt
 *
 * Registers 00h-0Eh are defined by the clock hardware.
 * The first 14-bytes support the clock functions.  They are ten read/write
 * data registers, and four status registers.  Two of the status
 * registers are read/write, and the other two are read only.
 *
 * It uses two ports: 70h and 71h.  To read a CMOS byte, 
 * execute an OUT to port 70h with the address of the byte to be read,
 * and execute an IN from port 71h to retrieve the requested information.
 * Bit 7 on port 70h is used to enable (0) or disable (1) Non-Maskable 
 * Interrupts.
 *
 * cmos[00h] - RTC seconds, BCD 00-59, Hex 00-3b
 * cmos[01h] - RTC second alarm, BCD 00-59, Hex 00-3b, other values ignored
 * cmos[02h] - RTC minutes, BCD 00-59, Hex 99-3b
 * cmos[03h] - RTC minute alarm, BCD 00-59, Hex 00-3b, other values ignored
 * cmos[04h] - RTC hours,
 *             BCD 00-23, hex 00-17 if 24 hour mode
 *             BCD 01-12, hex 01-0c if 12 hour AM
 *             BCD 81-92, hex 81-8c if 12 hour PM
 * cmos[05h] - RTC hour alarm, values same as cmos[04h], other values ignored
 * cmos[06h] - RTC day of week, 01-07 w/ Sunday=1
 * cmos[07h] - RTC day of month, BCD 01-31, hex 01-1f
 * cmos[08h] - RTC month, BCD 01-12, hex 01-0c
 * cmos[09h] - RTC year, BCD 00-99, hex 00-63
 *
 * BCD/Hex selection depends on bit-2 of register B (0bh).
 * 12/24 hour selection depends on bit-1 of register B (0bh).
 * Alarm will fire when contents of all three alarm byte registers match
 * their targets.
 *
 * cmos[0ah] - RTC status register A (read/write) (usually 26h)
 * cmos[0bh] - RTC status register B (read/write)
 * cmos[0ch] - RTC status register C (read only)
 * cmos[0dh] - RTC status register D (read only)
 *
 * Other registers.
 * cmos[0eh] - IBM PS/2 diagnostic status byte
 * cmos[0eh-13h ] - Amstrad time and date machine last used
 * cmos[0fh] - IBM reset code (IBM PS/2 "Shutdown Status Byte")
 *
 */

class CMOS_byte_t
{
public:
    virtual u8_t read() = 0;
    virtual void write( u8_t new_val ) = 0;
};

/***************************************************************************/

u8_t bin_to_bcd( u8_t bin )
{
    return (bin/10 << 4) | (bin % 10);
}

void rtc_t::do_update()
{
    word_t y, m, d, h, mins, s;

    last_update = backend_get_unix_seconds();
    unix_to_gregorian( get_last_update(), y, m, d, h, mins, s );

    year = y; month = m; day = d;
    hours = h; minutes = mins; seconds = s;
    week_day = day_of_week(year, month, day);
}

rtc_t rtc;

/***************************************************************************/

class CMOS_00_t : public CMOS_byte_t
{
public:
    u8_t read() { rtc.do_update();return bin_to_bcd( rtc.seconds ); }
    void write( u8_t new_val )  {}
};

class CMOS_01_t : public CMOS_byte_t
{
public:
    u8_t read() { return raw; }
    void write( u8_t new_val )  { raw = new_val; }

    u8_t raw;
};

class CMOS_02_t : public CMOS_byte_t
{
public:
    u8_t read() { return bin_to_bcd( rtc.minutes ); }
    void write( u8_t new_val )  { }
};

class CMOS_03_t : public CMOS_byte_t
{
public:
    u8_t read() { return raw; }
    void write( u8_t new_val )  { raw = new_val; }

    u8_t raw;
};

class CMOS_04_t : public CMOS_byte_t
{
public:
    u8_t read() { return bin_to_bcd( rtc.hours ); }
    void write( u8_t new_val )  {}
};

class CMOS_05_t : public CMOS_byte_t
{
public:
    u8_t read() { return raw; }
    void write( u8_t new_val )  { raw = new_val; }

    u8_t raw;
};

class CMOS_06_t : public CMOS_byte_t
{
public:
    u8_t read() { return bin_to_bcd( rtc.week_day ); }
    void write( u8_t new_val )  {}
};

class CMOS_07_t : public CMOS_byte_t
{
public:
    u8_t read() { return bin_to_bcd( rtc.day ); }
    void write( u8_t new_val )  {}
};

class CMOS_08_t : public CMOS_byte_t
{
public:
    u8_t read() { return bin_to_bcd( rtc.month ); }
    void write( u8_t new_val )  { }
};

class CMOS_09_t : public CMOS_byte_t
{
public:
    u8_t read() { return bin_to_bcd( rtc.year ); }
    void write( u8_t new_val )  { }
};

class CMOS_0a_t : public CMOS_byte_t
{
public:
    void write( u8_t new_val )  
	{ 
	    x.raw = new_val; 
	    rtc.set_periodic_frequency(32768 >> (x.fields.rate-1));
	}

    u8_t read()
    { 
	x.fields.update_cycle_in_progress = !x.fields.update_cycle_in_progress;
	if( !x.fields.update_cycle_in_progress ) {
	    rtc.do_update();
	}
	return x.raw; 
    }

    union {
	u8_t raw;
	struct {
	    u8_t rate : 4;
	    u8_t divider : 3;
	    u8_t update_cycle_in_progress : 1;
	} fields;
    } x;

    CMOS_0a_t()
    {
	x.raw = 0x26;
	rtc.set_periodic_frequency(32768 >> (x.fields.rate-1));
    }
};

class CMOS_0b_t : public CMOS_byte_t
{
public:
    u8_t read() { return x.raw; }

    void write( u8_t new_val )
    { 
	x.raw = new_val;
	if( x.fields.enable_alarm_interrupt)
	    printf( "WARNING: Unimplemented alarm timer used in the mc146818rtc model.\n");
	if( x.fields.enable_periodic_interrupt)
	{
	    rtc.enable_periodic();
	    printf( "Enabled periodic rtc timer frequency %d\n", rtc.get_periodic_frequency());
	}
	else
	{
	    rtc.disable_periodic();
	    printf( "Disabled periodic rtc timer\n");
	}
	    
    }

    union {
	u8_t raw;
	struct {
	    u8_t enable_daylight_savings : 1;
	    u8_t enable_24_hour : 1;
	    u8_t enable_binary : 1;
	    u8_t enable_square_wave : 1;
	    u8_t enable_update_ended_interrupt : 1;
	    u8_t enable_alarm_interrupt : 1;
	    u8_t enable_periodic_interrupt : 1;
	    u8_t enable_cycle_update : 1;
	} fields;
    } x;

    CMOS_0b_t()
    {
	x.raw = 0;
	x.fields.enable_24_hour = 1;
    }
};

class CMOS_0c_t : public CMOS_byte_t
{
public:
    u8_t read() { return x.raw; }
    void write( u8_t new_val )
	{ printf( "CMOS write to read-only 0xc register.\n"); }

    union {
	u8_t raw;
	struct {
	    u8_t : 4;
	    u8_t updated_ended_interrupt_flag : 1;
	    u8_t alarm_interrupt_flag : 1;
	    u8_t periodic_interrup_flag : 1;
	    u8_t interrupt_request_flag : 1;
	} fields;
    } x;
};

class CMOS_0d_t : public CMOS_byte_t
{
public:
    u8_t read() { return x.raw; }
    void write( u8_t new_val )
	{ printf( "CMOS write to read-only 0xd register.\n"); }

    union {
	u8_t raw;
	struct {
	    u8_t : 7;
	    u8_t good_battery : 1; // 1 indicates good battery.
	} fields;
    } x;
};

class CMOS_0e_t : public CMOS_byte_t
{
public:
    u8_t read()
	{ printf( "Unimplemented: CMOS byte 0x0e read.\n"); return 0; }
    void write( u8_t new_val )
	{ printf( "Unimplemented: CMOS byte 0x0e write, value %u\n",
		  new_val ); }
};

class CMOS_0f_t : public CMOS_byte_t
{
    u8_t val;
public:
    u8_t read()
	{ return val; }
    void write( u8_t new_val )
	{ val = new_val; }
};

class CMOS_10_t : public CMOS_byte_t
{
    /* No floppy */
public:
    u8_t read()
	{ printf( "Unimplemented: CMOS floppy\n"); return 0; }
    void write( u8_t new_val )
	{ printf( "Unimplemented: CMOS floppy\n", new_val); }

};

/***************************************************************************/

static CMOS_00_t CMOS_00;
static CMOS_01_t CMOS_01;
static CMOS_02_t CMOS_02;
static CMOS_03_t CMOS_03;
static CMOS_04_t CMOS_04;
static CMOS_05_t CMOS_05;
static CMOS_06_t CMOS_06;
static CMOS_07_t CMOS_07;
static CMOS_08_t CMOS_08;
static CMOS_09_t CMOS_09;
static CMOS_0a_t CMOS_0a;
static CMOS_0b_t CMOS_0b;
static CMOS_0c_t CMOS_0c;
static CMOS_0d_t CMOS_0d;
static CMOS_0e_t CMOS_0e;
static CMOS_0f_t CMOS_0f;
static CMOS_0f_t CMOS_10;

static CMOS_byte_t * CMOS_registers[] = {
    &CMOS_00, &CMOS_01, &CMOS_02, &CMOS_03, &CMOS_04, &CMOS_05,
    &CMOS_06, &CMOS_07, &CMOS_08, &CMOS_09, &CMOS_0a, &CMOS_0b,
    &CMOS_0c, &CMOS_0d, &CMOS_0e, &CMOS_0f, &CMOS_10
};

void mc146818rtc_portio( u16_t port, u32_t & value, bool read )
{
    static word_t addr_port;
    
    if (read)
	dprintf(debug_portio, "mc146818rtc portio read port %x\n", port);
    else
	dprintf(debug_portio,"mc146818rtc portio write port %x value %x\n", port, value);
	
       
    if( port == 0x70 ) {
	if( read )
	    value = addr_port;
	else
	    addr_port = value;
	return;
    }
    
    if (addr_port > 0x10)
    {
	dprintf(debug_portio, "Unimplemented: CMOS register %x\n", addr_port); 
	return; 
    }
    
    if( port == 0x71 ) {
	if( read )
	    value = CMOS_registers[ addr_port ]->read();
	else
	    CMOS_registers[ addr_port ]->write( value );
    }

    return;
}

#endif	/* CONFIG_DEVICE_PASSTHRU */
