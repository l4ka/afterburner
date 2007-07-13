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

static const int i82371_debug=1;

extern pci_header_t pci_i82371ab_header_dev0;
i82371ab_t i82371ab_dev0( &pci_i82371ab_header_dev0 );

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
	con << "i82371ab: write to reg " << reg << " value " << (void*)value << '\n';

    switch(reg) {
    case 0x0: pri_regs.bmicx.raw = (u8_t)value; break;
    case 0x2: pri_regs.bmisx.raw = (u8_t)value; break;
    case 0x4: pri_regs.dtba = value; break;

    case 0x8: sec_regs.bmicx.raw = (u8_t)value; break;
    case 0xa: sec_regs.bmisx.raw = (u8_t)value; break;
    case 0xc: sec_regs.dtba = value; break;

    default:
	con << "i82371ab: write to unknown register " << reg << '\n';
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
	con << "i82371ab: read from unknown register " << reg << '\n';
    }

    if(i82371_debug)
	con << "i82371ab: read from reg " << reg << " value " << (void*)value << '\n';
}
