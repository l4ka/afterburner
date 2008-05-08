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

#include <device/ide.h>
#include <device/rtc.h>
#include <device/portio.h>
#include <aftertime.h>
#include <console.h>
#include <debug.h>
#include INC_WEDGE(backend.h)


#if defined(CONFIG_DEVICE_PASSTHRU) && !defined(CONFIG_WEDGE_L4KA)

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
    virtual u8_t read(word_t port) = 0;
    virtual void write( word_t port, u8_t new_val ) = 0;
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
    u8_t read(word_t port) { return bin_to_bcd( rtc.seconds ); }
    void write(word_t port, u8_t new_val )  {}
};

class CMOS_01_t : public CMOS_byte_t
{
public:
    u8_t read(word_t port) { return raw; }
    void write(word_t port, u8_t new_val )  { raw = new_val; }

    u8_t raw;
};

class CMOS_02_t : public CMOS_byte_t
{
public:
    u8_t read(word_t port) { return bin_to_bcd( rtc.minutes ); }
    void write(word_t port, u8_t new_val )  { }
};

class CMOS_03_t : public CMOS_byte_t
{
public:
    u8_t read(word_t port) { return raw; }
    void write(word_t port, u8_t new_val )  { raw = new_val; }

    u8_t raw;
};

class CMOS_04_t : public CMOS_byte_t
{
public:
    u8_t read(word_t port) { return bin_to_bcd( rtc.hours ); }
    void write(word_t port, u8_t new_val )  {}
};

class CMOS_05_t : public CMOS_byte_t
{
public:
    u8_t read(word_t port) { return raw; }
    void write(word_t port, u8_t new_val )  { raw = new_val; }

    u8_t raw;
};

class CMOS_06_t : public CMOS_byte_t
{
public:
    u8_t read(word_t port) { return bin_to_bcd( rtc.week_day ); }
    void write(word_t port, u8_t new_val )  {}
};

class CMOS_07_t : public CMOS_byte_t
{
public:
    u8_t read(word_t port) { return bin_to_bcd( rtc.day ); }
    void write(word_t port, u8_t new_val )  {}
};

class CMOS_08_t : public CMOS_byte_t
{
public:
    u8_t read(word_t port) { return bin_to_bcd( rtc.month ); }
    void write(word_t port, u8_t new_val )  { }
};

class CMOS_09_t : public CMOS_byte_t
{
public:
    u8_t read(word_t port) { return bin_to_bcd( rtc.year ); }
    void write(word_t port, u8_t new_val )  { }
};

class CMOS_0a_t : public CMOS_byte_t
{
public:
    void write(word_t port, u8_t new_val )  
	{ 
	    x.raw = new_val; 
	    rtc.set_periodic_frequency(32768 >> (x.fields.rate-1));
	}

    u8_t read(word_t port)
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
    u8_t read(word_t port) { return x.raw; }

    void write(word_t port, u8_t new_val )
    { 
	x.raw = new_val;
	if( x.fields.enable_alarm_interrupt)
	    printf( "WARNING: Unimplemented alarm timer used in the mc146818rtc model.\n");
	if( x.fields.enable_periodic_interrupt)
	{
	    rtc.enable_periodic();
	    get_intlogic().raise_irq(mc146818_irq);
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
    u8_t read(word_t port) { return x.raw; }
    void write(word_t port, u8_t new_val )
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
    u8_t read(word_t port) { return x.raw; }
    void write(word_t port, u8_t new_val )
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
    u8_t read(word_t port)
	{ printf( "Unimplemented: CMOS byte 0x0e read.\n"); return 0; }
    void write(word_t port, u8_t new_val )
	{ printf( "Unimplemented: CMOS byte 0x0e write %x\n",
		  new_val ); }
};

class CMOS_0f_t : public CMOS_byte_t
{
    u8_t val;
public:
    u8_t read(word_t port)
	{ return val; }
    void write(word_t port, u8_t new_val )
	{ val = new_val; }
};

class CMOS_10_3f_t : public CMOS_byte_t
{
public:
    u8_t read(word_t port)
	{ 
	    word_t val;
	    switch (port)
	    {
	    case 0x10:
		//   10h 16 1 byte Floppy Disk Drive Types 
		//       Bits 7-4 = Drive 0 type 
		//       Bits 3-0 = Drive 1 type 
		//       0000 = None 
		//       0001 = 360KB 
		//       0010 = 1.2MB 
		//       0011 = 720KB 
		//       0100 = 1.44MB 
		val = 0;
		break;
	    case 0x11:
		//	 Bit 7 = Mouse support disable/enable 
		//       Bit 6 = Memory test above 1MB disable/enable 
		//       Bit 5 = Memory test tick sound disable/enable 
		//       Bit 4 = Memory parity error check disable/enable 
		//       Bit 3 = Setup utility trigger display disable/enable 
		//       Bit 2 = Hard disk type 47 RAM area (0:300h or upper 1KB of DOS area) 
		//       Bit 1 = Wait for <F1> if any error message disable/enable 
		//       Bit 0 = System boot up with Numlock (off/on) 
		val = 0xc4;
		break;
	    case 0x12:
		//     12h 18 1 byte Hard Disk Types 
		//       Bits 7-4 = Hard disk 0 type 
		//       Bits 3-0 = Hard disk 1 type 
		//       0000 = No drive installed 
		//       0001 = Type 1 installed 
		//       1110 = Type 14 installed 
		//       1111 = Type 16-47 (defined later in 19h) 
		//     TODO: align with device/ide.cc
		
#if defined(CONFIG_DEVICE_IDE)
		val = 
		    (ide.get_device(0)->present ? 0xf0 : 0x0) |
		    (ide.get_device(1)->present ? 0x0f : 0x0);
#else
		val = 0;
#endif
		break;
	    case 0x13:
		// 13h 19 1 byte Typematic Parameters 
		//       Bit 7 = typematic rate programming disable/enabled 
		//       Bit 6-5 = typematic rate delay 
		//       Bit 4-2 = Typematic rate 
		val = 0;
		break;
	    case 0x14:
		// 14h 20 1 byte Installed Equipment 
		//       Bits 7-6 = Number of floppy disks (00 = 1 floppy disk, 01 = 2 floppy disks) 
		//       Bits 5-4 = Primary display (00 = Use display adapter BIOS, 
		//				     01 = CGA 40 column, 
		//				     10 = CGA 80 column, 
		//				     11 = Monochrome Display Adapter) 
		//       Bit 3 = Display adapter installed/not installed 
		//       Bit 2 = Keyboard installed/not installed 
		//       Bit 1 = math coprocessor installed/not installed 
		//       Bit 0 = Always set to 1 
		val = 0x0f;
		break;
	    case 0x15:
		//   Base Memory Low Order Byte - Least significant byte
		val = 640 & 0xff; 
		break;
		
	    case 0x16:
		//	Base Memory High Order Byte - Most significant byte
		val = (640 >> 8) & 0xff; 
		break;
	    case 0x17:
		//	Extended Memory Low Order Byte - Least significant byte
		val = min(((resourcemon_shared.phys_size >> 10) - 0x400), 0xffffUL) & 0xff;
		break;
	    case 0x18:
		//	Extended Memory Low Order Byte - Most significant byte
		val = (min(((resourcemon_shared.phys_size >> 10) - 0x400), 0xffffUL) >> 8) & 0xff;
		break;
#if defined(CONFIG_DEVICE_IDE)
	    case 0x19:
		//	Hard Disk 0 Extended Type - (10h to 2Eh = Type 16 to 46 respectively)
	    case 0x1a:
		//	Hard Disk 0 Extended Type - (10h to 2Eh = Type 16 to 46 respectively)
		val = 0x2f;
		break;
	    case 0x1b:
		//	User Defined Drive C: - Number of cylinders least significant byte
		val = (ide.get_device(0)->present ? (ide.get_device(0)->cylinder & 0xff) : 0);
		break;
	    case 0x1c:
		//	User Defined Drive C: - Number of cylinder most significant byte
		val = (ide.get_device(0)->present ? ((ide.get_device(0)->cylinder >> 8) & 0xff) : 0);
		break;
	    case 0x1d:
		//	User Defined Drive C: - Number of heads	
		val = (ide.get_device(0)->present ? ide.get_device(0)->head : 0);
		break;
	    case 0x1e:
		//	User Defined Drive C: - Write precomp cylinder least significant byte
	    case 0x1f:
		//	User Defined Drive C: - Write precomp cylinder most significant byte
		val = 0xff;
		break;
	    case 0x20:
		//	User Defined Drive C: - Control byte
		val =  (ide.get_device(0)->present ? (0xc0 | ((ide.get_device(0)->head > 8) << 3)) : 0);
		break;
	    case 0x21:
		//	User Defined Drive C: - Landing zone least significant byte
		val = (ide.get_device(0)->present ? (ide.get_device(0)->cylinder & 0xff) : 0);
		break;
	    case 0x22:
		//	User Defined Drive C: - Landing zone most significant byte	
		val = (ide.get_device(0)->present ? ((ide.get_device(0)->cylinder >> 8) & 0xff) : 0);
		break;
	    case 0x23:
		//	User Defined Drive C: - Number of sectors	
		val = (ide.get_device(0)->present ? ide.get_device(0)->sector : 0);
		break;
	    case 0x24:
		//	User Defined Drive D: - Number of cylinders least significant byte
		val = (ide.get_device(1)->present ? (ide.get_device(1)->cylinder & 0xff) : 0);
		break;
	    case 0x25:
		//	User Defined Drive D: - Number of cylinder most significant byte
		val = (ide.get_device(1)->present ? ((ide.get_device(1)->cylinder >> 8) & 0xff) : 0);
		break;
	    case 0x26:
		//	User Defined Drive D: - Number of heads	
		val = (ide.get_device(1)->present ? ide.get_device(1)->head : 0);
		break;
	    case 0x27:
		//	User Defined Drive D: - Write precomp cylinder least significant byte
	    case 0x28:
		//	User Defined Drive D: - Write precomp cylinder most significant byte
		val = 0xff;
		break;
	    case 0x29:
		//	User Defined Drive D: - Control byte
		val =  (ide.get_device(1)->present ? (0xc0 | ((ide.get_device(1)->head > 8) << 3)) : 0);
		break;
	    case 0x2a:
		//	User Defined Drive D: - Landing zone least significant byte
		val = (ide.get_device(1)->present ? (ide.get_device(1)->cylinder & 0xff) : 0);
		break;
	    case 0x2b:
		//	User Defined Drive D: - Landing zone most significant byte	
		val = (ide.get_device(1)->present ? ((ide.get_device(1)->cylinder >> 8) & 0xff) : 0);
		break;
	    case 0x2c:
		//	User Defined Drive D: - Number of sectors	
		val = (ide.get_device(1)->present ? ide.get_device(1)->sector : 0);
		break;
#endif
	    case 0x2d:
		//	2Dh 45 1 byte System Operational Flags 
		//       Bit 7 = Weitek processor present/absent 
		//       Bit 6 = Floppy drive seek at boot enable/disable 
		//       Bit 5 = System boot sequence (1:hd, 0:fd)
		//       Bit 4 = System boot CPU speed high/low 
		//       Bit 3 = External cache enable/disable 
		//       Bit 2 = Internal cache enable/disable 
		//       Bit 1 = Fast gate A20 operation enable/disable 
		//       Bit 0 = Turbo switch function enable/disable 
		val = rtc.get_system_flags(); 
		break;
	    case 0x2e:
	    case 0x2f:
		//	2Eh 46 1 byte CMOS Checksum High Order Byte - Most significant byte 
		//	2Fh 47 1 byte CMOS Checksum Low Order Byte - Least significant byte 
		val = 0;
		break;
	    case 0x30:
		val = min(((resourcemon_shared.phys_size >> 10) - 0x400), 0xffffUL) & 0xff;
		break;
	    case 0x31:
		//	Extended Memory Low Order Byte - Most significant byte
		val = (min(((resourcemon_shared.phys_size >> 10) - 0x400), 0xffffUL) >> 8) & 0xff;
		break;
	    case 0x34:
		//	Actual Extended Memory (in 64KByte) - Least significant byte
		val = min(((resourcemon_shared.phys_size > 0x01000000) ? 
			   (resourcemon_shared.phys_size >> 16) - 0x100 : 0), 0xffffUL) & 0xff;
		break;
	    case 0x35:
		//	Actual Extended Memory (in 64KByte) - Most significant byte
		val = (min(((resourcemon_shared.phys_size > 0x01000000) ? 
			    (resourcemon_shared.phys_size >> 16) - 0x0100 : 0), 0xffffUL) >> 8) & 0xff;
	           
		break;
	    case 0x37:
		// 	37h 55 1 byte Password Seed and Color Option  
		// 	    Bit 7-4 = Password seed (do not change) 
		// 	    Bit 3-0 = Setup screen color palette 
		// 	    07h = White on black 
		// 	    70h = Black on white 
		// 	    17h = White on blue 
		// 	    20h = Black on green 
		// 	    30h = Black on turquoise 
		// 	    47h = White on red 
		// 	    57h = White on magenta 
		// 	    60h = Black on brown 
		val = 0;
		break;
	    case 0x38:
		//	eltorito boot sequence + boot signature check
		//	bits
		//	  0	floppy boot signature check (1: disabled, 0: enabled)
		//	7-4	boot drive #3 (0: unused, 1: fd, 2: hd, 3:cd, else: fd)
		val = 0x0;
		break;
	    case 0x39:
		//	ata translation policy - ata0 + ata1
		//	bits
		//	1-0	ata0-master (0: none, 1: LBA, 2: LARGE, 3: R-ECHS)
		//	3-2	ata0-slave
		//	5-4	ata1-master
		//	7-6	ata1-slave
		val = 0;
		break;
	    case 0x3a:
		//	ata translation policy - ata2 + ata3 (see above)
		val = 0;
		break;
	    case 0x3d:
		//	eltorito boot sequence (see above)
		//	bits
		//	3-0	boot drive #1
		//	7-4	boot drive #2
		val = 0x32;
		break;
	    default:
		__asm__ __volatile__ ("outb %b1, %0\n" : : "dn"(0x70), "a"(port) );
		__asm__ __volatile__ ("inb %1, %b0\n" : "=a"(val) : "dN"(0x71) );
		printf("mc146818rtc portio unsupported port %x hw/val %x\n", port, val);
		DEBUGGER_ENTER("UNIMPLEMENTED"); 
		break;
	    }
	    return val;
	}
    
    void write(word_t port, u8_t new_val )
	{ 
	    switch (port)
	    {
	    case 0x2d:
		rtc.set_system_flags(new_val); 
	    break;
	    default:
		printf( "Unimplemented: CMOS write port %x val %x\n", port, new_val); 
		break;
	    }
	}
};


/* Afterburner CMOS extensions */
class CMOS_80_t : public CMOS_byte_t
{
    u8_t val;
public:
    u8_t read(word_t port)
	{ 
	    vcpu_t &vcpu = get_vcpu();
	    word_t paddr = vcpu.get_wedge_paddr();
	    val = (paddr >> 20); 
	    return val; 
	}
    void write(word_t port, u8_t new_val )
	{  }
};

class CMOS_81_t : public CMOS_byte_t
{
    u8_t val;
public:
    u8_t read(word_t port)
	{ 
	    vcpu_t &vcpu = get_vcpu();
	    word_t paddr = ROUND_UP((vcpu.get_wedge_end_paddr() - vcpu.get_wedge_paddr()), MB(4));
	    val = (paddr >> 20); 
	    return val; 
	}
    void write(word_t port, u8_t new_val )
	{  }
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
static CMOS_10_3f_t CMOS_10_3f;

static CMOS_byte_t * CMOS_registers[] = {
    &CMOS_00, &CMOS_01, &CMOS_02, &CMOS_03, 
    &CMOS_04, &CMOS_05, &CMOS_06, &CMOS_07, 
    &CMOS_08, &CMOS_09, &CMOS_0a, &CMOS_0b,
    &CMOS_0c, &CMOS_0d, &CMOS_0e, &CMOS_0f, 
    &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, 
    &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, 
    &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, 
    &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, 
    &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, 
    &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, 
    &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, 
    &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, 
    &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, 
    &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, 
    &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, 
    &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, &CMOS_10_3f, 
};

static CMOS_80_t CMOS_80;
static CMOS_81_t CMOS_81;

static CMOS_byte_t * CMOS_ab_registers[] = {
   &CMOS_80, &CMOS_81,
};
    

void mc146818rtc_portio( u16_t port, u32_t & value, bool read )
{
    static word_t addr_port;
    
       
    if( port == 0x70 ) {
	if( read )
	    value = addr_port;
	else
	    addr_port = value;
	return;
    }

    if( port == 0x71 ) 
    {
	if (addr_port <= 0x3f)
	{
	    if( read )
		value = CMOS_registers[ addr_port ]->read(addr_port);
	    else
		CMOS_registers[ addr_port ]->write(addr_port, value);
	    
	}
	else if (addr_port >= 0x80 && addr_port <= 0x81)
	{
	    if( read )
		value = CMOS_ab_registers[ addr_port - 0x80 ]->read(addr_port);
	    else
		CMOS_ab_registers[ addr_port - 0x80 ]->write(addr_port, value);

	}	
	else 
	{
	    if (read)
	    {
		__asm__ __volatile__ ("outb %b1, %0\n" : : "dn"(0x70), "a"(addr_port) );
		__asm__ __volatile__ ("inb %1, %b0\n" : "=a"(value) : "dN"(0x71) );
	    }
	    printf("mc146818rtc portio %c unsupported port %x hw/val %x\n", 
		   (read ? 'r' : 'w'), addr_port, value);
	    DEBUGGER_ENTER("UNIMPLEMENTED");
	    return; 
	}
    }

    return;
}

#endif	/* CONFIG_DEVICE_PASSTHRU */
