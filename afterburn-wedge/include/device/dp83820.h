/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/device/dp83820.h
 * Description:   Declarations for the DP83820 device model.
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
#ifndef __AFTERBURN_WEDGE__INCLUDE__DEVICE__DP83820_H__
#define __AFTERBURN_WEDGE__INCLUDE__DEVICE__DP83820_H__

#if defined(CONFIG_DEVICE_DP83820)

#include <device/pci.h>
#include <l4/kip.h>

#include INC_ARCH(sync.h)
#include INC_ARCH(cpu.h)
#include INC_ARCH(intlogic.h)

#include INC_WEDGE(dp83820.h)
#include INC_WEDGE(backend.h)

extern "C" void __attribute__((regparm(2))) dp83820_write_patch( word_t value, word_t addr );
extern "C" word_t __attribute__((regparm(1))) dp83820_read_patch( word_t addr );


struct dp83820_cr_t {
    union {
	u32_t raw;
	struct {
	    u32_t tx_enable : 1;
	    u32_t tx_disable : 1;
	    u32_t rx_enable : 1;
	    u32_t rx_disable : 1;
	    u32_t tx_reset : 1;
	    u32_t rx_reset : 1;
	    u32_t unused1 : 1;
	    u32_t software_int : 1;
	    u32_t reset : 1;
	    u32_t tx_queue_select : 4;
	    u32_t rx_queue_select : 4;
	    u32_t unused2 : 15;
	} fields;
    } x;
} __attribute__((packed));

struct dp83820_cfg_t {
    union {
	u32_t raw;
	struct {
	    u32_t big_endian : 1;
	    u32_t ext_125 : 1;
	    u32_t brom_dis : 1;
	    u32_t pesel : 1;
	    u32_t exd : 1;
	    u32_t pow : 1;
	    u32_t sb : 1;
	    u32_t reqalg : 1;
	    u32_t extended_status_enable : 1;
	    u32_t phy_dis : 1;
	    u32_t phy_rst : 1;
	    u32_t master_64bit_enable : 1;
	    u32_t data_64bit_enable : 1;
	    u32_t pc64_detected : 1;
	    u32_t target_64bit_enable : 1;
	    u32_t mwi_dis : 1;
	    u32_t mrm_dis : 1;
	    u32_t timer_test_mode : 1;
	    u32_t phy_interrupt_ctrl : 3;
	    u32_t reserved1 : 1;
	    u32_t mode_1000 : 1;
	    u32_t reserved2 : 1;
	    u32_t tbi_en : 1;
	    u32_t reserved3 : 3;
	    u32_t full_duplex_status : 1;
	    u32_t speed_status : 2;
	    u32_t link_status : 1;
	} fields;
    } x;
} __attribute__((packed));

struct dp83820_irq_t {
    union {
	u32_t raw;
	struct {
	    u32_t rx_ok : 1;
	    u32_t rx_desc : 1;
	    u32_t rx_err : 1;
	    u32_t rx_early : 1;
	    u32_t rx_idle : 1;
	    u32_t rx_overrun : 1;
	    u32_t tx_ok : 1;
	    u32_t tx_desc : 1;
	    u32_t tx_err : 1;
	    u32_t tx_idle : 1;
	    u32_t tx_underrun : 1;
	    u32_t mib_service : 1;
	    u32_t software_int : 1;
	    u32_t pme : 1;
	    u32_t phy : 1;
	    u32_t high_bits_int : 1;
	    u32_t rx_fifo_overrun : 1;
	    u32_t rtabt : 1;
	    u32_t rmabt : 1;
	    u32_t sserr : 1;
	    u32_t dperr : 1;
	    u32_t rx_reset_complete : 1;
	    u32_t tx_reset_complete : 1;
	    u32_t rxdesc0 : 1;
	    u32_t rxdesc1 : 1;
	    u32_t rxdesc2 : 1;
	    u32_t rxdesc3 : 1;
	    u32_t txdesc0 : 1;
	    u32_t txdesc1 : 1;
	    u32_t txdesc2 : 1;
	    u32_t txdesc3 : 1;
	    u32_t reserved : 1;
	} fields;
    } x;
} __attribute__((packed));

struct dp83820_desc_t {
    word_t link;
    word_t bufptr;
    u16_t size;
    union {
	struct {
	    u16_t length_err : 1;
	    u16_t loopback_pkt : 1;
	    u16_t frame_alignment_err : 1;
	    u16_t crc_err : 1;
	    u16_t invalid_symbol_err : 1;
	    u16_t runt_pkt : 1;
	    u16_t long_pkt : 1;
	    u16_t dst_class : 2;
	    u16_t rx_overrun : 1;
	    u16_t rx_aborted : 1;
	    u16_t ok : 1;
	    u16_t crc : 1;
	    u16_t intr : 1;
	    u16_t more : 1;
	    u16_t own : 1;
	} rx;
	struct {
	    u16_t collision_cnt : 4;
	    u16_t excessive_collisions : 1;
	    u16_t out_of_window_collision : 1;
	    u16_t excessive_deferral : 1;
	    u16_t tx_deferred : 1;
	    u16_t carrier_sense_lost : 1;
	    u16_t tx_fifo_underrun : 1;
	    u16_t tx_abort : 1;
	    u16_t ok : 1;
	    u16_t crc : 1;
	    u16_t intr : 1;
	    u16_t more : 1;
	    u16_t own : 1;
	} tx;
	u16_t raw;
    } cmd_status;
    u16_t vlan_tag;
    struct {
	u16_t vlan_pkt : 1;
	u16_t ip_pkt : 1;
	u16_t ip_checksum_err : 1;
	u16_t tcp_pkt : 1;
	u16_t tcp_checksum_err : 1;
	u16_t udp_pkt : 1;
	u16_t udp_checksum_err : 1;
	u16_t unused : 9;
    } extended_status;

    bool device_rx_own() { return cmd_status.rx.own == 0; }
    void device_rx_release() { cmd_status.rx.own = 1; }
    bool driver_rx_own() { return cmd_status.rx.own == 1; }

    bool device_tx_own() { return cmd_status.tx.own == 1; }
    void device_tx_release() { cmd_status.tx.own = 0; }
    bool driver_tx_own() { return cmd_status.tx.own == 0; }

} __attribute__((packed));

class dp83820_t {
private:
    pci_header_t *pci_header;
    u8_t lan_address[6];
    bool backend_connect;
    word_t irq_pending_init;
    volatile word_t *irq_pending;
    word_t flush_cnt;
    u64_t flush_start;

    enum regs_e {
	CR=0x00/4, CFG=0x04/4, MEAR=0x08/4, PTSCR=0x0c/4, ISR=0x10/4,
	IMR=0x14/4, IER=0x18/4, IHR=0x1c/4, TXDP=0x20/4, TXDP_HI=0x24/4,
	TXCFG=0x28/4, GPIOR=0x2c/4, RXDP=0x30/4, RXDP_HI=0x34/4,
	RXCFG=0x38/4, PQCR=0x3c/4, WCSR=0x40/4, PCR=0x44/4, RFCR=0x48/4,
	RFDR=0x4c/4, SRR=0x58/4, VRCR=0xbc/4, VTCR=0xc0/4, VDR=0xc4/4,
	CCSR=0xcc/4, TBICR=0xe0/4, TBISR=0xe4/4, TANAR=0xe8/4, TANLPAR=0xec/4,
	TANER=0xf0/4, TESR=0xf4/4, last=0xf8/4
    };

    u32_t local_regs[last];
    u32_t *regs;

public:
    dp83820_backend_t backend;

private:
    volatile word_t *get_volatile_regs()
	{ return (volatile word_t *)regs; }

public:
    static const word_t reg_mask = KB(4)-1;

    dp83820_t( pci_header_t *new_pci_header )
    {
	regs = local_regs;
	backend_connect = false;
	flush_cnt = 0;
	irq_pending_init = 0;
	irq_pending = &irq_pending_init;
	pci_header = new_pci_header;
	lan_address[0] = 2;	// local
	*(u32_t *)(lan_address+2) = 0xdeadbeef;
	global_simple_reset();
    }

    void global_simple_reset();

    void rx_enable();
    void tx_enable();
    void txdp_absorb();
    void backend_init();
    bool backend_rx_async_idle( word_t *irq );
    void backend_cancel_rx_async_idle();
    void backend_raise_async_irq();
    void backend_flush( bool going_idle );

    void raise_irq() {
	if( !*irq_pending && (regs[ISR] & regs[IMR]) && regs[IER] )
	{
	    *irq_pending = 1;
	    get_intlogic().raise_irq( get_irq() );
	}
    }
    void set_rx_ok_irq()
	{ bit_set_atomic( 0, get_volatile_regs()[ISR] ); }
    void set_rx_desc_irq()
	{ bit_set_atomic( 1, get_volatile_regs()[ISR] ); }
    void set_rx_idle_irq()
	{ bit_set_atomic( 4, get_volatile_regs()[ISR] ); }

    void clear_irq_pending()
	{ *irq_pending = 0; }

    void write_word( word_t value, word_t reg );
    word_t read_word( word_t reg );

    word_t get_irq()
	{ return pci_header->x.fields.interrupt_line; }
    word_t get_bus_addr()
	{ return pci_header->x.fields.base_addr_registers[1].get_mem_address();}

    bool has_extended_status()
	{ return ((dp83820_cfg_t *)&regs[CFG])->x.fields.extended_status_enable == 1; }

    pci_header_t *get_pci_header()
	{ return pci_header; }
    
    static const char *get_name()
	{ return "dp83820"; }
    static dp83820_t * get_device( word_t index )
    {
	extern dp83820_t dp83820_dev0;
	return &dp83820_dev0;
    }
    static dp83820_t * get_pfault_device( word_t pfault_addr )
    {
	dp83820_t *dev = get_device(0);
	if( dev->backend.is_window_pfault(pfault_addr) )
	    return dev;
	return NULL;
    }

    static void flush_devices( bool going_idle )
    {
	get_device(0)->backend_flush( going_idle );
    }

    word_t get_rxdp()
	{ return get_volatile_regs()[RXDP]; }

    dp83820_desc_t *get_rx_desc_virt()
	{ return (dp83820_desc_t *)backend_phys_to_virt( get_rxdp() ); }

    bool atomic_next_rxdp( word_t old_rxdp, word_t new_rxdp )
    {
	word_t actual = cmpxchg( &get_volatile_regs()[RXDP], old_rxdp,
		new_rxdp );
	return actual == old_rxdp;
    }

    dp83820_desc_t *claim_next_rx_desc()
    {
	for( ;; ) {
	    word_t rxdp = get_rxdp();
	    if( !rxdp )
		return NULL;
	    dp83820_desc_t *d = (dp83820_desc_t *)backend_phys_to_virt(rxdp);
	    if( !d->device_rx_own() )
		return NULL; // No more buffers assigned to the device.
	    // TODO: it is incorrect to write a NULL pointer into RXDP.
	    if( atomic_next_rxdp(rxdp, d->link) )
		return d;
	}
    }

    word_t get_txdp()
	{ return regs[TXDP]; }

    dp83820_desc_t *get_tx_desc_virt()
	{ return (dp83820_desc_t *)backend_phys_to_virt( get_txdp() ); }
    dp83820_desc_t *get_tx_desc_next( dp83820_desc_t *d )
    {
	if( d->link == 0 )
	    return NULL;
	return (dp83820_desc_t *)backend_phys_to_virt( d->link );
    }

};

#define ON_DEVICE_DP83820(a) a

#else

#define ON_DEVICE_DP83820(a) do {} while(0)

#endif	/* CONFIG_DEVICE_DP83820 */

#endif	/* __AFTERBURN_WEDGE__INCLUDE__DEVICE__DP83820_H__ */
