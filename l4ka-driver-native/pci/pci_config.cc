/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     pci/pci_config.cc
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

#include <common/console.h>
#include <pci/pci_config.h>

#undef PCI_DEBUG
#if defined PCI_DEBUG
# define pci_dbg(val...) printf(val)
#else
# define pci_dbg(val...)
#endif

void PciScanner::scan_devices( void )
{
    //this->scan_device_tree( pci_get_start_bus() );
    for (unsigned i = 0; i < 32; i++)
	this->scan_device_tree( i );
	     
}

bool PciScanner::scan_device_tree( pci_bus_addr_t bus )
{
    pci_dev_addr_t dev;
    u16_t vendor_id;
    u8_t header_type;
    pci_func_addr_t pci_max_func;

    for( dev = pci_get_start_dev(); pci_is_valid_dev(dev); dev++ ) {
	vendor_id = pci_get_vendor_id( bus, dev, 0 );

	if( !pci_is_valid_vendor(vendor_id) )
	    continue;

	header_type = pci_get_header_type( bus, dev, 0 );

	pci_max_func = 0;
	if ((header_type & 0x80) != 0) 
	    pci_max_func = 7;

	if( (header_type & 0x7F) == 1 ) {
	    if ((header_type & 0x80) != 0)
		printf("PCI: warning: multifunction bridge not supported\n");
	    continue;
	    /*
	    for (pci_func_addr_t func = 0; func <= pci_max_func; func ++) {

		vendor_id = pci_get_vendor_id( bus, dev, func );
		if( !pci_is_valid_vendor(vendor_id) )
		    continue;

		pci_bus_addr_t sub_bus = pci_read8( bus, dev, func, 0x19 );

		pci_dbg ("PCI: sub bus is: %x @ bus %x dev %x func %x \n", sub_bus, bus, dev, func);

		if( !this->scan_device_tree(sub_bus) )
		    return false;
	    }
	    */
	}
	else if( (header_type & 0x7F) == 0 ) {
	    for (pci_func_addr_t func = 0; func <= pci_max_func; func ++) {

		vendor_id = pci_get_vendor_id( bus, dev, func );
		if( !pci_is_valid_vendor(vendor_id) )
		    continue;
		
		pci_dbg ("PCI: device @ bus %x dev %x func %x \n", bus, dev, func);

		PciDeviceConfig pci_config;
		pci_config.init( bus, dev, func );
		if( !this->inspect_device( &pci_config ) )
		    return false;
	    }
	}
    }

    return true;
}

