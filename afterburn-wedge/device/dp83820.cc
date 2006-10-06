/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/device/dp83820.cc
 * Description:   Front-end support for the DP83820 device model.
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

#include INC_ARCH(cpu.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(backend.h)

#include <device/dp83820.h>
#include <memory.h>
#include <burn_counters.h>
#include <profile.h>

static const bool debug_memio=0;
static const bool debug_regio=0;

DECLARE_BURN_COUNTER_REGION(dp83820_regs, 0x400/4);

extern pci_header_t pci_dp83820_header_dev0;
dp83820_t dp83820_dev0( &pci_dp83820_header_dev0 );


extern "C" void __attribute__((regparm(2)))
dp83820_write_patch( word_t value, word_t addr )
{
    // TODO: confirm that the address maps to our bus address.  And determine
    // the dp83820 device from the address.
    if( debug_memio )
	con << "dp83820 write @ " << (void *)addr 
	    << ", value " << (void *)value 
	    << ", ip " << __builtin_return_address(0) << '\n';

    word_t word = (addr & dp83820_t::reg_mask) / 4;

    INC_BURN_REGION_WORD(dp83820_regs, word);
    ON_INSTR_PROFILE(
	    instr_profile_return_addr( (word_t)__builtin_return_address(0) ));

    dp83820_t::get_device(0)->write_word( value, word );
}

extern "C" word_t __attribute__((regparm(1)))
dp83820_read_patch( word_t addr )
{
    // TODO: confirm that the address maps to our bus address.  And determine
    // the dp83820 device from the address.
    word_t word = (addr & dp83820_t::reg_mask) / 4;

    INC_BURN_REGION_WORD(dp83820_regs, word);
    ON_INSTR_PROFILE(
	    instr_profile_return_addr( (word_t)__builtin_return_address(0) ));

    word_t value = dp83820_t::get_device(0)->read_word( word );

    if( debug_memio )
	con << "dp83820 read @ " << (void *)addr
	    << ", value " << (void *)value 
	    << ", ip " << __builtin_return_address(0) << '\n';

    return value;
}

void dp83820_t::write_word( word_t value, word_t reg )
{
    if( debug_regio )
	con << "dp83820 register write, register " << reg
	    << " (" << (void *)reg << "), value " 
	    << (void *)value << '\n';

    switch( reg ) {
    case CR: {
	dp83820_cr_t *cr = (dp83820_cr_t *)&value;
	if( value & ~0x5 ) {
	    if( cr->x.fields.tx_disable ) 
		cr->x.fields.tx_disable = cr->x.fields.tx_enable = 0;
	    else if( cr->x.fields.tx_enable )
		tx_enable();
	    if( cr->x.fields.rx_disable )
		cr->x.fields.rx_disable = cr->x.fields.rx_enable = 0;
	    else if( cr->x.fields.rx_enable )
		rx_enable();
	    if( cr->x.fields.tx_reset ) {
		cr->x.fields.tx_reset = 0;
		dp83820_irq_t *irq = (dp83820_irq_t *)&regs[ISR];
		irq->x.fields.tx_reset_complete = 1;
		raise_irq();
	    }
	    if( cr->x.fields.rx_reset ) {
		cr->x.fields.rx_reset = 0;
		dp83820_irq_t *irq = (dp83820_irq_t *)&regs[ISR];
		irq->x.fields.rx_reset_complete = 1;
		raise_irq();
	    }
	    if( cr->x.fields.software_int ) {
		cr->x.fields.software_int = 0;
		dp83820_irq_t *irq = (dp83820_irq_t *)&regs[ISR];
		irq->x.fields.software_int = 1;
		raise_irq();
	    }
	    if( cr->x.fields.reset ) {
		backend_init();
		global_simple_reset();
		cr->x.raw = 0;
	    }
	}
	else if( cr->x.fields.tx_enable )
	    tx_enable();
	else if( cr->x.fields.rx_enable )
	    rx_enable();
	break;
    }
    case ISR:
	return;
    case PTSCR:
	value = 1 << 9;	// BIST completed.
	break;
    case CFG: {
	dp83820_cfg_t *cfg = (dp83820_cfg_t *)&value;
	cfg->x.fields.mode_1000 = 1;
	cfg->x.fields.full_duplex_status = 0; // Reverse polarity.
	cfg->x.fields.link_status = backend_connect;
	cfg->x.fields.speed_status = 1; // Reverse polarity.
	break;
    }
    case RFCR:
	if( value == 0 )
	    regs[ RFDR ] = lan_address[0] | (lan_address[1] << 8);
	else if( value == 2 )
	    regs[ RFDR ] = lan_address[2] | (lan_address[3] << 8);
	else if( value == 4 )
	    regs[ RFDR ] = lan_address[4] | (lan_address[5] << 8);
	break;
    }

    if( reg < last )
	regs[ reg ] = value;

    if( EXPECT_FALSE(TXDP == reg) )
	txdp_absorb();
}

word_t dp83820_t::read_word( word_t reg )
{
    word_t value = 0;

    if( reg < last )
	value = regs[reg];

    if( ISR == reg ) {
	clear_irq_pending();
	regs[ ISR ] = 0; // On read, the ISR register clears.
    }

    if( debug_regio )
	con << "dp83820 register read, register " << reg 
	    << " (" << (void *)reg << ")"
	    << ", value " << (void *)value << '\n';

    return value;
}

void dp83820_t::global_simple_reset()
    // NOTE: no side effects outside this class.  This function is called
    // from the C++ constructor.
{
    memzero( regs, sizeof(regs) );

    dp83820_cfg_t cfg = {x:{raw:0}};
    cfg.x.fields.mode_1000 = 1;
    cfg.x.fields.full_duplex_status = 0; // Reverse polarity.
    cfg.x.fields.link_status = backend_connect;
    cfg.x.fields.speed_status = 1; // Reverse polarity.
    regs[ CFG ] = cfg.x.raw;

    regs[ MEAR ] = 0x12;
    regs[ ISR ] = 0x00608000;
    regs[ TXCFG ] = 0x120;
    regs[ RXCFG ] = 0x4;
//    regs[ BRAR ] 0xffffffff;
    regs[ SRR ] = 0x0103;
//    regs[ MIBC ] = 0x2;
    regs[ TESR ] = 0xc000;
}

