/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/device/portio.cc
 * Description:   Direct PC99 port accesses to
 *                their device models, or directly to the devices if so
 *                configured.
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

#include <device/portio.h>
#include <console.h>
#include INC_WEDGE(backend.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(iostream.h)
    
extern DEBUG_STREAM con_driver;

/* To see a list of the fixed I/O ports, see section 6.3.1 in the 
 * Intel 82801BA ICH2 and 82801BAM ICH2-M Datasheet.
 */

UNUSED static bool do_passthru_portio( u16_t port, u32_t &value, bool read, u32_t bit_width )
{
    if (!read)
	dprintf(debug_portio, "passthru portio port write %x val %x\n", port, value);
    
    if( read ) {
	u32_t tmp;
	switch( bit_width ) {
	    case 8:
		__asm__ __volatile__ ("inb %1, %b0\n" 
			: "=a"(tmp) : "dN"(port) );
		value = tmp;
		break;
	    case 16:
		__asm__ __volatile__ ("inw %1, %w0\n" 
			: "=a"(tmp) : "dN"(port) );
		value = tmp;
		break;
	    case 32:
		__asm__ __volatile__ ("inl %1, %0\n" 
			: "=a"(tmp) : "dN"(port) );
		value = tmp;
		break;
	default:
		return false;
	}
    }
    else {
	switch( bit_width ) {
	    case 8:
		__asm__ __volatile__ ("outb %b0, %1\n" 
			: : "a"(value), "dN"(port) );
		break;
	    case 16:
		__asm__ __volatile__ ("outw %w0, %1\n" 
			: : "a"(value), "dN"(port) );
		break;
	    case 32:
		__asm__ __volatile__ ("outl %0, %1\n" 
			: : "a"(value), "dN"(port) );
		break;
	default:
	    return false;
	}
    }
    
    if (read)
	dprintf(debug_portio, "passthru portio port read %x val %x\n", port, value);
    return true;
}

static bool do_portio( u16_t port, u32_t &value, bool read, u32_t bit_width )
{
    if( read )
	value = 0xffffffff;

    switch( port )
    {
	case 0x80:
	    // Often used as the debug port.  Linux uses it for a delay.
	    // Some DMA controllers claim this port.
#if defined(CONFIG_DEVICE_PASSTHRU_0x80)
	    return do_passthru_portio( port value, read, bit_width );
#else
	    return true;
#endif

	// Programmable interrupt controller
	case 0x20 ... 0x21:
	case 0xa0 ... 0xa1:
	    i8259a_portio( port, value, read );
	    return true;

	// Programmable interval timer
	case 0x40 ... 0x43:
	    i8253_portio( port, value, read );
	    return true;

	case 0x61: // NMI status and control register.  Keyboard port.
	    legacy_0x61( port, value, read );
	    return true;

	case 0x60:
	case 0x62 ... 0x64: // keyboard
#if defined(CONFIG_DEVICE_PASSTHRU_KEYBOARD)
	    return do_passthru_portio( port, value, read, bit_width );
#else
	    i8042_portio( port, value, read );
	    return true;
#endif
	case 0x238 ... 0x23f: // Bus mouse
#if defined(CONFIG_DEVICE_PASSTHRU_KEYBOARD)
	    return do_passthru_portio( port, value, read, bit_width );
#else
	    return true;
#endif

	case 0x70 ... 0x7f: // RTC
	    mc146818rtc_portio( port, value, read );
	    return true;

	case 0x3f8 ... 0x3ff: // COM1
#if defined(CONFIG_DEVICE_PASSTHRU_COM1)
	    return do_passthru_portio( port, value, read, bit_width );
#else
	    serial8250_portio( port, value, read );
	    return true;
#endif
	case 0x2e8 ... 0x2ef: // COM2
#if defined(CONFIG_DEVICE_PASSTHRU_COM2)
	    return do_passthru_portio( port, value, read, bit_width );
#else
	    serial8250_portio( port, value, read );
	    return true;
#endif
	case 0x2f8 ... 0x2ff: // COM3
#if defined(CONFIG_DEVICE_PASSTHRU_COM3)
	    return do_passthru_portio( port, value, read, bit_width );
#else
	    serial8250_portio( port, value, read );
	    return true;
#endif
	case 0x3e8 ... 0x3ef: // COM4
#if defined(CONFIG_DEVICE_PASSTHRU_COM4)
	    return do_passthru_portio( port, value, read, bit_width );
#else
	    serial8250_portio( port, value, read );
	    return true;
#endif

	case 0x1f0 ... 0x1f7:   // Primary IDE controller
	case 0x170 ... 0x177:   // Secondary IDE controller
	case 0xb400 ... 0xb407: // Third IDE controller
	case 0xb408 ... 0xb40f: // Fourth IDE controller
	case 0x376:
	case 0x3f6:
#if defined(CONFIG_DEVICE_PASSTHRU_IDE)
	    return do_passthru_portio( port, value, read, bit_width );
#elif defined(CONFIG_DEVICE_IDE)
	    ide_portio( port, value, read );
#endif
	    return true;
	case 0x377: // Floppy disk controller 2
#if defined(CONFIG_DEVICE_PASSTHRU_FLOPPY)
	    return do_passthru_portio( port, value, read, bit_width );
#else
	    return true;
#endif
	case 0x3f7: // Floppy disk controller 1
#if defined(CONFIG_DEVICE_PASSTHRU_FLOPPY)
	    return do_passthru_portio( port, value, read, bit_width );
#else
	    return true;
#endif

	case 0x3c0 ... 0x3df: // VGA
#if defined(CONFIG_DEVICE_PASSTHRU_VGA)
	    return do_passthru_portio( port, value, read, bit_width );
#else
	    return true;
#endif

#if defined(CONFIG_DEVICE_PASSTHRU_PCI)
	case 0xcf8 ... 0xcff: // PCI configuration mechanism 1
	    return do_passthru_portio( port, value, read, bit_width );
	// 0xcf8 ... 0xcfa: PCI config mechanism 2, deprecated as of PCI v 2.1
	case 0xc000 ... 0xcfff: // PCI configuration mechanism 2
	    return do_passthru_portio( port, value, read, bit_width );
#endif

#if defined(CONFIG_DEVICE_I82371AB)
    case 0xb000 ... 0xb00f: // IDE Bus-Master interface
	i82371ab_portio( port, value, read );
	return true;
#endif

    case 0x400 ... 0x403: // BIOS debug ports
	//con_driver.print_char(value);
	L4_KDB_PrintChar(value);
	return true;

	default:
#if defined(CONFIG_DEVICE_PASSTHRU)
	    // Aargh, until we enable passthru access to the ports
	    // claimed by PCI devices via their configuration registers,
	    // we need a global pass through.
	    return do_passthru_portio( port, value, read, bit_width );
#endif
	    return false;
    }


    return true;
}

bool portio_read( u16_t port, u32_t &value, u32_t bit_width )
{
    value = ~0;
#if defined(CONFIG_DEVICE_PCI) // Virtual PCI.
    if( (port >= 0xc000) && (port < 0xd000) )
	return true; // PCI config with type 2 accesses --- unsupported

    switch( port ) {
	case 0xcf8: pci_config_address_read( value, bit_width ); return true;
	case 0xcfc: pci_config_data_read( value, bit_width, 0 ); return true;
	case 0xcfd: pci_config_data_read( value, bit_width, 8 ); return true;
	case 0xcfe: pci_config_data_read( value, bit_width, 16 ); return true;
	case 0xcff: pci_config_data_read( value, bit_width, 24 ); return true;
    }

    if( (port >= 0xcf8) && (port < 0xd00) )
	return false; // Ignore other PCI accesses for now.
#endif

    return do_portio( port, value, true, bit_width );
}

bool portio_write( u16_t port, u32_t value, u32_t bit_width )
{
#if defined(CONFIG_DEVICE_PCI) // Virtual PCI
    if( (port >= 0xc000) && (port < 0xd000) )
	return false; // PCI config with type 2 accesses --- unsupported

    switch( port ) {
	case 0xcf8: pci_config_address_write( value, bit_width ); return true;
	case 0xcfc: pci_config_data_write( value, bit_width, 0 ); return true;
	case 0xcfd: pci_config_data_write( value, bit_width, 8 ); return true;
	case 0xcfe: pci_config_data_write( value, bit_width, 16 ); return true;
	case 0xcff: pci_config_data_write( value, bit_width, 24 ); return true;
    }

    if( (port >= 0xcf8) && (port < 0xd00) )
	return false; // Ignore other PCI accesses for now.
#endif

    return do_portio( port, value, false, bit_width );
}

