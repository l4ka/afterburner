
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

