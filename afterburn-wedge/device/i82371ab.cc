/*********************************************************************
 *
 * Copyright (C) 2007,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/device/ide.cc
 * Description:   Front-end emulation for the 82371AB PCI-TO-ISA /
 *                IDE XCELERATOR (PIIX4)
 * Note:          This implementation only covers the IDE Controller part  
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

#include <device/i82371ab.h>

static const int i82371_debug=0;

extern pci_header_t pci_i82371ab_header_dev0;
i82371ab_t i82371ab_dev0( &pci_i82371ab_header_dev0 );
extern ide_t ide;

void i82371ab_t::do_portio( u16_t port, u32_t & value, bool read )
{
    u32_t bmib_addr = (pci_header->x.fields.base_addr_registers[4].x.io.address << 2);

    if(read)
	read_register( port - bmib_addr, value);
    else
	write_register( port - bmib_addr, value);
}


void i82371ab_t::write_register( u16_t reg, u32_t &value )
{
    if(i82371_debug)
	printf( "i82371ab: write to reg " << reg << " value " << (void*)value << '\n';

    switch(reg) {
    case 0x0: pri_regs.bmicx.raw = (u8_t)value; 
	if(value & 0x1)
	    ide.ide_start_dma(pri_dev, !pri_regs.bmicx.fields.rwcon);
	break;
    case 0x2: pri_regs.bmisx.raw = (u8_t)value;
	if(value & 0x2)
	    pri_regs.bmisx.fields.dma_err = 0;
	if(value & 0x4)
	    pri_regs.bmisx.fields.ideints = 0;
	break;
    case 0x4: pri_regs.dtba = value; break;

    case 0x8: sec_regs.bmicx.raw = (u8_t)value;
	if(value & 0x1)
	    ide.ide_start_dma(sec_dev, !sec_regs.bmicx.fields.rwcon);
	break;
    case 0xa: sec_regs.bmisx.raw = (u8_t)value; 
	if(value & 0x2)
	    sec_regs.bmisx.fields.dma_err = 0;
	if(value & 0x4)
	    sec_regs.bmisx.fields.ideints = 0;
	break;
    case 0xc: sec_regs.dtba = value; break;

    default:
	printf( "i82371ab: write to unknown register " << reg << '\n';
    }
}


void i82371ab_t::read_register ( u16_t reg, u32_t &value )
{

    switch(reg) {
    case 0x0:
	value = pri_regs.bmicx.raw; break;
    case 0x2: 
	value = pri_regs.bmisx.raw; break;
    case 0x4: 
	value = pri_regs.dtba; break;

    case 0x8:
	value = sec_regs.bmicx.raw; break;
    case 0xa: 
	value = sec_regs.bmisx.raw; break;
    case 0xc:
	value = sec_regs.dtba; break;

    default:
	printf( "i82371ab: read from unknown register " << reg << '\n';
    }

    if(i82371_debug)
	printf( "i82371ab: read from reg " << reg << " value " << (void*)value << '\n';
}


bool i82371ab_t::is_dma_enabled(u16_t drive)
{
    switch(drive) {
    case 0: return pci_region->x.fields.udma_ctrl.x.fields.psde0; break;
    case 1: return pci_region->x.fields.udma_ctrl.x.fields.psde1; break;
    case 2: return pci_region->x.fields.udma_ctrl.x.fields.ssde0; break;
    case 3: return pci_region->x.fields.udma_ctrl.x.fields.ssde1; break;
    default: return false;
    }
    return false;
}


bool i82371ab_t::get_rwcon(u16_t drive)
{
    if(drive&0x2)
	return sec_regs.bmicx.fields.rwcon;
    else
	return pri_regs.bmicx.fields.rwcon;
}


bool i82371ab_t::get_ssbm(u16_t drive)
{
    if(drive&0x2)
	return sec_regs.bmicx.fields.ssbm;
    else
	return pri_regs.bmicx.fields.ssbm;
}


void i82371ab_t::set_dma_transfer_error(u16_t drive)
{
    if(drive&0x2) {
	sec_regs.bmisx.fields.ideints=0;
	sec_regs.bmisx.fields.bmidea=0;
    }
    else {
	pri_regs.bmisx.fields.ideints=0;
	pri_regs.bmisx.fields.bmidea=0;
    }
}


void i82371ab_t::set_dma_start(u16_t drive)
{
    if(drive&0x2) {
	sec_regs.bmisx.fields.bmidea = 1;
	sec_regs.bmisx.fields.dma_err = 0;
	sec_regs.bmisx.fields.ideints = 0;
    }
    else {
	if(pri_regs.bmisx.fields.ideints)
	    printf( "WARNING start dma w/o int ack\n");
	pri_regs.bmisx.fields.bmidea = 1;
	pri_regs.bmisx.fields.dma_err = 0;
	pri_regs.bmisx.fields.ideints = 0;
    }
}


void i82371ab_t::set_dma_end(u16_t drive)
{
    if(drive&0x2) {
	sec_regs.bmisx.fields.ideints=1;
	sec_regs.bmisx.fields.bmidea=0;
    }
    else {
	pri_regs.bmisx.fields.ideints=1;
	pri_regs.bmisx.fields.bmidea=0;
    }
}


u32_t i82371ab_t::get_dtba(u16_t drive)
{
    if(!drive)
	return pri_regs.dtba;
    else
	return sec_regs.dtba;
}


// return physical descriptor table
prdt_entry_t *i82371ab_t::get_prdt_entry(u16_t drive)
{
    prdt_entry_t *prdt = (prdt_entry_t*)(pri_regs.dtba);
    prdt->transfer.fields.reserved = 0;
    if(i82371_debug) {
	printf( "Physical Region Descriptor Table:\n");
	for(int i=0;i<512;i++) {
	    printf( "Transfer " << (prdt+i)->transfer.fields.count << " bytes to " << (void*)(prdt+i)->base_addr << '\n';
	    if( (prdt+i)->transfer.fields.eot)
		break;
	}
    }
    return prdt;
}


bool i82371ab_t::is_dev_pfault(u32_t pfault_addr)
{
#if !defined(CONFIG_L4KA_VT)
    if(  ((pfault_addr & PAGE_MASK) == pri_regs.dtba ) || ((pfault_addr & PAGE_MASK) == sec_regs.dtba ) )
    	return true;
    else
#endif
	return false;
}


void i82371ab_t:: register_device(u16_t drive, ide_device_t *dev)
{
    if(drive&0x2)
	sec_dev = dev;
    else
	pri_dev = dev;
}
