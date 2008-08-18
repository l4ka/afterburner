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
#include <device/rtc.h>
#include <console.h>
#include INC_WEDGE(backend.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(iostream.h)
    
extern DEBUG_STREAM con_driver;

#if defined(CONFIG_QEMU_DM)
#include <l4ka/qemu_dm.h>
extern qemu_dm_t qemu_dm;
#endif

/* To see a list of the fixed I/O ports, see section 6.3.1 in the 
 * Intel 82801BA ICH2 and 82801BAM ICH2-M Datasheet.
 */

UNUSED static void do_passthru_portio( const u16_t port, u32_t &value, const bool read, const u32_t bit_width )
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
	    break;
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
	    break;
	}
    }
    
    if (read)
	dprintf(debug_portio, "passthru portio port read %x val %x\n", port, value);
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
	do_passthru_portio( port, value, read, bit_width );
#endif 
	break;
    case 0x92: // Gate a20
	legacy_0x92( port, value, read );
	break;

	// Programmable interrupt controller
    case 0x20 ... 0x21:
    case 0xa0 ... 0xa1:
    case 0x4d0 ... 0x4d1:
	i8259a_portio( port, value, read );
	break;

	// Programmable interval timer
    case 0x40 ... 0x43:
	i8253_portio( port, value, read );
	break;

    case 0x61: // NMI status and control register.  Keyboard port.
#if defined(CONFIG_DEVICE_PASSTHRU_KEYBOARD)
	do_passthru_portio( port, value, read, bit_width );
#else
	legacy_0x61( port, value, read );
#endif
	break;

    case 0x60:
    case 0x62 ... 0x64: // keyboard
#if defined(CONFIG_DEVICE_PASSTHRU_KEYBOARD)
	do_passthru_portio( port, value, read, bit_width );
#else
	i8042_portio( port, value, read );
#endif
	break;
	    
    case 0xe9: //Plex/Bochs BIOS e9 hack
	con_driver.print_char(value);
	return true;
	
    case 0x238 ... 0x23f: // Bus mouse
#if defined(CONFIG_DEVICE_PASSTHRU_KEYBOARD)
	do_passthru_portio( port, value, read, bit_width );
#endif
	break;
    case 0x70 ... 0x7f: // RTC
	mc146818rtc_portio( port, value, read );
	return true;

    case 0x279: // ISAPNP Port Enumerator
    case 0xa79: // ISAPNP Port Enumerator
	do_passthru_portio( port, value, read, bit_width );
	//dprintf(debug_portio_unhandled, "isapnp portio %c port %x val %d width %d\n",
	//(read ? 'r' : 'w'), port, value, bit_width);
	break;
    case 0x3f8 ... 0x3ff: // COM1
#if defined(CONFIG_L4KA_HVM)
	// i30pc4 ttyS1 0xc800
	do_passthru_portio( port, value, read, bit_width );
	//do_passthru_portio( 0xc800 + (port & 0x7), value, read, bit_width);
#elif defined(CONFIG_DEVICE_PASSTHRU_COM1)
	do_passthru_portio( port, value, read, bit_width );
#else
	serial8250_portio( port, value, read );
#endif
	break;
    case 0x2f8 ... 0x2ff: // COM2
#if defined(CONFIG_DEVICE_PASSTHRU_COM2)
	do_passthru_portio( port, value, read, bit_width );
#else
	serial8250_portio( port, value, read );
#endif
	break;
    case 0x3e8 ... 0x3ef: // COM3
#if defined(CONFIG_DEVICE_PASSTHRU_COM3)
	do_passthru_portio( port, value, read, bit_width );
#else
	serial8250_portio( port, value, read );
#endif
	break;
    case 0x2e8 ... 0x2ef: // COM4
#if defined(CONFIG_DEVICE_PASSTHRU_COM4)
	do_passthru_portio( port, value, read, bit_width );
#else
	serial8250_portio( port, value, read );
#endif
	break;

    case 0x1f0 ... 0x1f7:   // Primary IDE controller
    case 0x170 ... 0x177:   // Secondary IDE controller
    case 0xb400 ... 0xb407: // Third IDE controller
    case 0xb408 ... 0xb40f: // Fourth IDE controller
    case 0x376:
    case 0x3f6:
#if defined(CONFIG_DEVICE_PASSTHRU_IDE)
	do_passthru_portio( port, value, read, bit_width );
#elif defined(CONFIG_DEVICE_IDE)
	ide_portio( port, value, read );
#endif
	break;
    case 0x377: // Floppy disk controller 2
    case 0x3f2 ... 0x3f5: // Floppy 
    case 0x3f7: // Floppy disk controller 1
#if  defined(CONFIG_DEVICE_PASSTHRU_FLOPPY)
	do_passthru_portio( port, value, read, bit_width );
#else
	dprintf(debug_portio_unhandled+1, "vfdc portio %c port %x val %d width %d\n",
		(read ? 'r' : 'w'), port, value, bit_width);
#endif
	break;
    case 0x1ce ... 0x1cf: // VGA
    case 0x3b0 ... 0x3df: // VGA
	do_passthru_portio( port, value, read, bit_width );
	break;

    case 0xcf8 ... 0xcff: // PCI configuration mechanism 1
#if defined(CONFIG_DEVICE_PASSTHRU_PCI)
	do_passthru_portio( port, value, read, bit_width );
#elif defined(CONFIG_DEVICE_PCI)
	pci_portio(port, value, read, bit_width);
#else
	dprintf(debug_portio_unhandled, "unhandled pci portio %c port %x val %d width %d\n",
		(read ? 'r' : 'w'), port, value, bit_width);
	return false;
#endif
	break;

#if defined(CONFIG_DEVICE_I82371AB)
    case 0xc000 ... 0xc00f: // IDE Bus-Master interface
	i82371ab_portio( port, value, read );
	break;
#endif

#if defined(CONFIG_DEVICE_PASSTHRU_PCI)
    case 0xc000 ... 0xcfff: // PCI configuration mechanism 2
	do_passthru_portio( port, value, read, bit_width );
#endif
	break;

#if defined(CONFIG_WEDGE_L4KA)
    case 0x400 ... 0x403: // BIOS debug ports
	//con_driver.print_char(value);
	L4_KDB_PrintChar(value);
#endif
	break;
    default:

    case 0x808 ... 0x808: // PM Timer
	do_passthru_portio( port, value, read, bit_width );
	break;

#if defined(CONFIG_DEVICE_PASSTHRU)
	// Until we enable passthru access to the ports
	// claimed by PCI devices via their configuration registers,
	// we need a global pass through.
	do_passthru_portio( port, value, read, bit_width );
#endif
	dprintf(debug_portio_unhandled, "unhandled portio %c port %x val %d width %d\n",
		(read ? 'r' : 'w'), port, value, bit_width);

	return false;
    }
    return true;
}

bool portio_read( u16_t port, u32_t &value, u32_t bit_width )
{
#if defined (CONFIG_QEMU_DM)
    switch(port)
    {
	PASS_THROUGH_PORTS
	default:
	    unsigned long count = 1;
	    L4_Word_t size = (bit_width >> 3);
	    uint8_t dir = IOREQ_READ;
	    L4_Word_t df = 0; //ignored
	    L4_Word_t value_is_ptr = 0;
	    L4_Word_t v = value;
	    if(!qemu_dm.send_pio(port, count, size,v,dir,df,value_is_ptr))
		return 0;
	    value = v;
	    return 1;
    }
#endif
    return do_portio( port, value, true, bit_width );

}

bool portio_write( u16_t port, u32_t value, u32_t bit_width )
{
#if defined (CONFIG_QEMU_DM)
    switch(port)
    {
	PASS_THROUGH_PORTS
	default:
	    unsigned long count = 1;
	    L4_Word_t size = (bit_width >> 3);
	    uint8_t dir = IOREQ_WRITE;
	    L4_Word_t df = 0; //ignored
	    L4_Word_t value_is_ptr = 0;
	    L4_Word_t v = value;
	    return qemu_dm.send_pio(port, count, size,v,dir,df,value_is_ptr);
    }
#endif
    return do_portio( port, value, false, bit_width );

}

template <typename T>
INLINE bool do_portio_string_read( word_t port, word_t bytes, word_t mem)
{
    T *buf = (T*)mem;
    
    word_t tmp, i;
    
    for ( i=0; i < (bytes /  sizeof(T)) ;i++) 
    {
	if (!do_portio( port, tmp, true, sizeof(T)*8) )
	    return false;
	*(buf++) = (T)tmp;
    }
    return true;
}

template <typename T>
INLINE bool do_portio_string_write( word_t port, word_t bytes, word_t mem)
{
    T *buf = (T*)mem;
    
    word_t tmp, i;
    
    for( i=0; i < (bytes /  sizeof(T)) ; i++) 
    {
	tmp = (u32_t) *(buf++);
	if (!do_portio( port, tmp, false, sizeof(T)*8) )
	    return false;
    }
    return true;
}

bool portio_string_read(word_t port, word_t mem, word_t count, word_t bit_width, bool df)
{
    word_t pmem = 0, psize = 0;
    bool ret = true;
    word_t size = count * bit_width / 8;
    word_t n = 0;
#if defined (CONFIG_QEMU_DM)	
    switch(port)
    {
	PASS_THROUGH_PORTS
	default:
	    backend_resolve_kaddr(mem,size,pmem,psize);
	    size = (bit_width >> 3);
	    uint8_t dir = IOREQ_READ;
	    L4_Word_t value_is_ptr = 1;
	    L4_Word_t v = mem;
	    if( !qemu_dm.send_pio(port, count, size,v,dir,df,value_is_ptr) )
	    {
		dprintf(debug_id_t(0,0), " unhandled string IO %x mem %x (p %x)\n", port, mem, pmem);
		DEBUGGER_ENTER("UNTESTED");
	    }
	    return 1;
    }
#endif

    while (n < size)
    {

	backend_resolve_kaddr(mem + n, size - n, pmem, psize);
	
	//dprintf(debug_portio+1, "read string port %x count %d mem %x size %d/%d (pmem %x psize %d)\n", 
	//port, count, mem + n, n, size, pmem, psize);
	    
	ret = true;
	
	switch( bit_width ) 
	{
	case 8:
	    ret = do_portio_string_read<u8_t>(port, psize, pmem);
	    break;
	case 16:
	    ret = do_portio_string_read<u16_t>(port, psize, pmem);
	    break;
	case 32:
	    ret = do_portio_string_read<u32_t>(port, psize, pmem);
	    break;
	}
	    
	if (!ret)
	{
	    dprintf(debug_id_t(0,0), " unhandled string IO %x mem %x (p %x)\n", port, mem, pmem);
	    DEBUGGER_ENTER("UNTESTED");
	    break;
	}
	    
	n += psize;
    }
    return ret;
}

bool portio_string_write(word_t port, word_t mem, word_t count, word_t bit_width,bool df)
{
    word_t pmem = 0, psize = 0;
    bool ret = true;
    word_t size = count * bit_width / 8;
    word_t n = 0;
	
#if defined (CONFIG_QEMU_DM)	
    switch(port)
    {
	PASS_THROUGH_PORTS
	
	default:
	    backend_resolve_kaddr(mem,size,pmem,psize);
	    size = (bit_width >> 3);
	    uint8_t dir = IOREQ_WRITE;
	    L4_Word_t value_is_ptr = 1;
	    L4_Word_t v = mem;
	    if( !qemu_dm.send_pio(port, count, size,v,dir,df,value_is_ptr) )
	    {
		dprintf(debug_id_t(0,0), " unhandled string IO %x mem %x (p %x)\n", port, mem, pmem);
		DEBUGGER_ENTER("UNTESTED");
	    }
	    return 1;
    }
#endif

    while (n < size)
    {

	backend_resolve_kaddr(mem + n, size - n, pmem, psize);
	
	//dprintf(debug_portio+1, "write string port %x count %d mem %x size %d/%d (pmem %x psize %d)\n", 
	//port, count, mem + n, n, size, pmem, psize);
	    
	ret = true;
	
	switch( bit_width ) 
	{
	case 8:
	    ret = do_portio_string_write<u8_t>(port, psize, pmem);
	    break;
	case 16:
	    ret = do_portio_string_write<u16_t>(port, psize, pmem);
	    break;
	case 32:
	    ret = do_portio_string_write<u32_t>(port, psize, pmem);
	    break;
	}
	    
	if (!ret)
	{
	    dprintf(debug_id_t(0,0), " unhandled string IO %x mem %x (p %x)\n", port, mem, pmem);
	    DEBUGGER_ENTER("UNTESTED");
	    break;
	}
	    
	n += psize;
    }
    return ret;
}
