
#ifndef __PCI__PCI_CONFIG_H__
#define __PCI__PCI_CONFIG_H__

/*
 *
 * Configuration address space.
 * - geographical addressing of devices: bus number, device number, function 
 *   number
 *   o bus number is 8-bits, valid bus numbers are 0..255
 *   o device numbers use 5-bits, permitting 32 unique device numbers
 *   o function numbers are 3-bits, permitting 8 functions per device.  The
 *     first function must be 0, for scanning purposes.
 * - each function within a device is required to implement the following
 *   registers:
 *   o vendor ID - the device's manufacturer
 *   o device ID - manufacturer's assigned part number
 *   o revision ID - the revision of the part
 *   o class code - identifies a PCI device's generic function
 *   o header type - identifies a PCI function's predefined header type
 * - other common config registers:
 *   o base address - identifies a request for either I/O or memory
 *   o interrupt pin - identifies a request for an interrupt line
 *   o interrupt line - programmed with an interrupt line number
 *   o command - controls parity and system error responses
 *   o status - records the assertion of SERR# and PERR#
 *
 * - PCI configuration space format, 256 bytes:
 *   o device independent header:
 *     + predefined header, 16 common bytes
 *     + layout dependent on header type, 48 bytes (typically), 2 layouts
 *       defined: standard device, PCI/PCI bridge
 *   o device dependent, 192 bytes
 *
 *   o Type 00h header region
 *     00h	vendor ID
 *     02h	device ID
 *     04h	command
 *     06h	status
 *     08h	revision ID
 *     09h	class code: programming interface
 *     0Ah	class code: sub-class code
 *     0Bh	class code: base class code
 *     0Ch	cache line size
 *     0Dh	latency timer
 *     0Eh	header type
 *     0Fh	BIST
 *     10h - 27h	base address registers
 *     28h - 2Bh	CardBus CIS pointer
 *     2Ch - 2Dh	subsystem vendor ID
 *     2Eh - 2Fh	subsystem ID
 *     30h		expansion ROM base address
 *     34h - 3Bh	reserved
 *     3Ch		interrupt line
 *     3Dh		interrupt pin
 *     3Eh		min_gnt
 *     3Fh		max_lat
 *
 * - scanning:
 *   o the first function within a device must be function 0.
 *   o an unimplemented function returns value 0xffff for the vendor ID
 *   o algorithm:
 *     1. read vendor ID register of function 0.  If a value other than 0xffff
 *        is read, the device is present.
 *     2. test for a multi-function device: read the header type register
 *        (offset 0xe); if bit 7 is set it is a multi-function device.
 *     3. for a multi-function device, read the vendor ID register of each
 *        function and test for 0xffff.
 *
 * - configuration space access, mechanism #1
 *   o CONFIG_ADDRESS register, I/O location 0xcf8
 *   o CONFIG_DATA register, I/O location 0xcfc
 *
 *   o PCI bus access is disabled when CONFIG_ADDRESS is set to 0.
 *
 *   CONFIG_ADDRESS:
 *     0-1 : 00
 *     2-7 : register number
 *     8-10  : function number
 *     11-15 : device number
 *     16-23 : bus number
 *     24-30 : reserved
 *     31 : enable bit
 */

#include "pci_port.h"

#define PCI_COMMAND_INVALIDATE	0x10

typedef enum {
    pci_vendor_id = 0x00,
    pci_device_id = 0x02,
    pci_command = 0x04,
    pci_status = 0x06,
    pci_revision_id = 0x08,
    pci_class_code = 0x09,
    pci_cache_line_size = 0x0c,
    pci_latency_timer = 0x0d,
    pci_header_type = 0x0e,
    pci_base_addr0 = 0x10,
    pci_base_addr1 = 0x14,
    pci_base_addr2 = 0x18,
    pci_base_addr3 = 0x1c,
    pci_base_addr4 = 0x20,
    pci_base_addr5 = 0x24,
    pci_cardbus_pointer = 0x28,
    pci_subsystem_vendor_id = 0x2c,
    pci_subsystem_id = 0x2e,
    pci_rom_addr = 0x30,
    pci_interrupt_line = 0x3c,
    pci_interrupt_pin = 0x3d,
    pci_min_gnt = 0x3e,
    pci_max_gnt = 0x3f,
} pci_config_addr_e;

INLINE bool pci_is_valid_vendor( u16_t vendor )
{
    return vendor < 0xffff;
}

INLINE pci_bus_addr_t pci_get_start_bus( void )
{
    return 0;
}

INLINE pci_dev_addr_t pci_get_start_dev( void )
{
    return 0;
}

INLINE bool pci_is_valid_dev( pci_dev_addr_t dev )
{
    return dev < 32;
}

INLINE bool pci_is_valid_func( pci_func_addr_t func )
{
    return func < 8;
}

INLINE u16_t pci_get_vendor_id( pci_bus_addr_t bus, pci_dev_addr_t dev, pci_func_addr_t func )
{
    return pci_read16( bus, dev, func, pci_vendor_id );
}

INLINE u16_t pci_get_device_id( pci_bus_addr_t bus, pci_dev_addr_t dev, pci_func_addr_t func )
{
    return pci_read16( bus, dev, func, pci_device_id );
}

INLINE u8_t pci_get_header_type( pci_bus_addr_t bus, pci_dev_addr_t dev, pci_func_addr_t func )
{
    return pci_read8( bus, dev, func, pci_header_type );
}

class PciBaseRegister
{
protected:
    union {
	struct {
	    u32_t status : 1;
	    u32_t type : 2;
	    u32_t prefetchable : 1;
	    u32_t base : 28;
	} mem;
	struct {
	    u32_t status : 1;
	    u32_t reserved : 1;
	    u32_t base : 30;
	} io;
	u32_t raw;
    };

public:
    bool is_io()    { return this->io.status == 1; }
    bool is_mem()   { return this->mem.status == 0; }
    bool is_valid() { return this->raw != 0; }
    bool is_64()    { return this->mem.type == 2; }

    void *get_raw()  { return (void *)this->raw; }

    void *base()
    { 
	return (void *)(this->is_io() ? (this->raw & 0xfffffffc):(this->raw & 0xfffffff0) );
    }

    u16_t base16()
    {
	return (u16_t)(this->is_io() ? (this->raw & 0x0fffc):(this->raw & 0x0fff0) );
    }

    void init( u32_t raw ) { this->raw = raw; }
    PciBaseRegister() { this->raw = 0; }
};

class PciDeviceConfig
{
protected:
    pci_bus_addr_t bus;
    pci_dev_addr_t dev;
    pci_func_addr_t func;
    PciBaseRegister base_registers[6];

public:
    void init( PciDeviceConfig *src )
	{ this->init( src->get_bus(), src->get_dev(), src->get_func() ); }

    void init( pci_bus_addr_t bus, pci_dev_addr_t dev, pci_func_addr_t func )
	{ this->bus = bus; this->dev = dev; this->func = func; }

    pci_bus_addr_t get_bus() { return this->bus; }
    pci_dev_addr_t get_dev() { return this->dev; }
    pci_func_addr_t get_func() { return this->func; }

    int get_base_reg_tot() { return 6; }
    PciBaseRegister &get_base_reg( int which ) 
	{ return this->base_registers[which]; }
    void read_base_registers()
    {
	int which = 0;
	while( which < 6 ) {
	    u32_t raw = pci_read32( this->bus, this->dev, this->func, 
		    4*which+pci_base_addr0 );
	    this->base_registers[which].init( raw );
	    if( this->base_registers[which].is_mem() && 
		    this->base_registers[which].is_64() )
		which += 2;
	    else
		which += 1;
	}
    }

    u16_t get_device_id()
    {
	return pci_read16( this->bus, this->dev, this->func, pci_device_id ); 
    }

    u16_t get_vendor_id()
    {
	return pci_read16( this->bus, this->dev, this->func, pci_vendor_id ); 
    }

    u8_t get_revision_id()
    {
	return pci_read8( this->bus, this->dev, this->func, pci_revision_id );
    }

    u16_t get_subsystem_id()
    {
	return pci_read16( this->bus, this->dev, this->func, pci_subsystem_id );
    }

    u16_t get_subsystem_vendor_id()
    {
	return pci_read16( this->bus, this->dev, this->func, pci_subsystem_vendor_id );
    }

    u32_t get_command()
    {
	return pci_read32( this->bus, this->dev, this->func, pci_command );
    }

    u8_t get_interrupt_line()
    {
	return pci_read8( this->bus, this->dev, this->func, pci_interrupt_line );
    }

    u8_t get_interrupt_pin()
    {
	// 0 = interrupt pin not used
	// [1-4] = [A-D]
	return pci_read8( this->bus, this->dev, this->func, pci_interrupt_pin );
    }

    u16_t read_config16( pci_config_addr_t reg )
	{ return pci_read16( this->bus, this->dev, this->func, reg ); }

    void write_config16( pci_config_addr_t reg, u16_t value )
    {
	pci_write16( this->bus, this->dev, this->func, reg, value );
    }

    u8_t read_config8( pci_config_addr_t reg )
	{ return pci_read8( this->bus, this->dev, this->func, reg ); }

    void write_config8( pci_config_addr_t reg, u8_t value )
    {
	pci_write8( this->bus, this->dev, this->func, reg, value );
    }

    void clear_mwi()
    {
	u16_t command;
	command = this->read_config16( pci_command );
	command &= ~PCI_COMMAND_INVALIDATE;
	this->write_config16( pci_command, command );
    }

    void set_mwi()
    {
	u16_t command;
	command = this->read_config16( pci_command );
	command |= PCI_COMMAND_INVALIDATE;
	this->write_config16( pci_command, command );
    }
};

class PciScanner
{
public:
    void scan_devices( void );
protected:
    virtual bool inspect_device( PciDeviceConfig *pci_config ) = 0;
    bool scan_device_tree( pci_bus_addr_t bus );
};

#endif	/* __PCI__PCI_CONFIG_H__ */
