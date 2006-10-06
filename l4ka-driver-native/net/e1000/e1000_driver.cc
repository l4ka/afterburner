
#include <l4/sigma0.h>

#include <common/resourcemon.h>
#include <common/valloc.h>
#include <common/ia32/page.h>
#include <common/ia32/bitops.h>

#include <net/net.h>
#include <net/e1000/e1000_driver.h>
#include <net/e1000/e1000.h>
#include <net/lanaddress_server.h>

#define cpu_to_le64(a)	((u64_t)(a))
#define cpu_to_le32(a)	((u32_t)(a))
#define cpu_to_le16(a)	((u16_t)(a))
#define le64_to_cpu(a)	((u64_t)(a))
#define le32_to_cpu(a)	((u32_t)(a))
#define le16_to_cpu(a)	((u16_t)(a))

/****************************************************************************/

bool E1000Driver::map_device()
{
    L4_Fpage_t request_fpage, rcv_fpage, result_fpage;
    static const log2size_t log2size = 17; // 128KB
    static const word_t pci_size = 1 << log2size;

    request_fpage = L4_Fpage( (u32_t)this->e1000_hw.hw_addr, pci_size );
    ASSERT( L4_Address(request_fpage) == (u32_t)this->e1000_hw.hw_addr );

    word_t e1000_base = get_valloc()->alloc_aligned( log2size );
    rcv_fpage = L4_Fpage( e1000_base, pci_size );
    ASSERT( L4_Address(rcv_fpage) == e1000_base );

    L4_Set_Rights( &request_fpage, L4_Readable | L4_Writable );
    L4_Set_Rights( &rcv_fpage, L4_Readable | L4_Writable );

    nprintf( 2, "e1000[%d]: requesting phys %lx --> virt %lx\n", this->which,
	    L4_Address(request_fpage), L4_Address(rcv_fpage) );

    result_fpage = L4_Sigma0_GetPage_RcvWindow( L4_nilthread, request_fpage,
	    rcv_fpage );

    /* TODO: complete error detection!! */
    if( L4_IsNilFpage(result_fpage) )
	return false;

    this->e1000_hw.hw_addr = (uint8_t *)L4_Address(rcv_fpage);
    return true;
}

bool E1000Driver::init( int which, PciDeviceConfig *new_pci_config )
{
    SMP_OP( this->cpu_lock = 0 );
    SMP_OP( this->active = 0 );
    SMP_OP( this->event_pending = false );
    SMP_OP( this->irq_pending = false );

    this->promiscuous = 0;
    this->iface_cnt = 0;
    this->lan_addr_bitmap = 0;
    this->iface_list = NULL;
    this->e1000_hw.back = this;
    this->which = which;
    this->pci_config.init( new_pci_config );
    this->last_csum_start = 0;
    this->last_csum_offset = 0;

    this->server_shared = get_server_shared( get_index() );
    this->server_shared->x.irq_status = 0;
    this->server_shared->x.irq_pending = 0;

    nprintf( 1, "e1000[%d]: Initializing device %x:%x, %x:%x\n", this->which,
	    this->pci_config.get_bus(), this->pci_config.get_dev(),
	    this->pci_config.get_vendor_id(),
	    this->pci_config.get_device_id() );

#if 0
    unsigned irq_no = IrqScanner.LookupIrq( &this->pci_config );
    if( irq_no == 0 )
    {
	printf( "e1000[%d]: unable to find the interrupt mapping\n",
		this->which );
	return false;
    }
#else
    unsigned irq_no = this->pci_config.get_interrupt_line();
#endif
    this->irq_tid = L4_GlobalId( irq_no, 1 );
    nprintf( 1, "e1000[%d]: interrupt: %d\n", this->which, irq_no );

    this->pci_config.read_base_registers();
    this->e1000_hw.hw_addr = (uint8_t *)this->pci_config.get_base_reg(0).base();
    if( !this->map_device() ) {
	printf( "e1000[%d]: error mapping device into virtual address space!\n",
		this->which );
	enter_kdebug( "sigma0 failure" );
    }

    this->e1000_hw.io_base = 0;
    for( int i = 0; i < this->pci_config.get_base_reg_tot(); i++ ) {
	if( this->pci_config.get_base_reg(i).is_io() ) {
	    this->e1000_hw.io_base = (uint32_t)this->pci_config.get_base_reg(i).base();
	    break;
	}
    }

    nprintf( 2, "e1000[%d]: hw_addr %p\n", this->which, this->e1000_hw.hw_addr);
    nprintf( 2, "e1000[%d]: io base %x\n", this->which, this->e1000_hw.io_base);

    this->e1000_hw.vendor_id = this->pci_config.get_vendor_id();
    this->e1000_hw.device_id = this->pci_config.get_device_id();
    this->e1000_hw.revision_id = this->pci_config.get_revision_id();
    this->e1000_hw.subsystem_vendor_id = this->pci_config.get_subsystem_vendor_id();
    this->e1000_hw.subsystem_id = this->pci_config.get_subsystem_id();
    this->e1000_hw.pci_cmd_word = this->pci_config.get_command();

    this->mtu = 1500;
    this->e1000_hw.max_frame_size = this->mtu + ENET_HEADER_SIZE +
	ETHERNET_FCS_SIZE;
    this->e1000_hw.min_frame_size = MINIMUM_ETHERNET_FRAME_SIZE;

    if( e1000_set_mac_type(&this->e1000_hw) )
	printf( "e1000[%d]: unknown MAC type\n", this->which );
    else
	nprintf( 2, "e1000[%d]: mac type: %x\n",
		this->which, this->e1000_hw.mac_type );
    if( this->e1000_hw.mac_type < e1000_82543 ) {
	printf( "e1000[%d]: unsupported NIC (mac type %x)\n", this->which,
		this->e1000_hw.mac_type );
	L4_KDB_Enter( "oops, unsupported Intel gigabit card!" );
	return false;
    }


    this->e1000_hw.fc_high_water = E1000_FC_HIGH_THRESH;
    this->e1000_hw.fc_low_water = E1000_FC_LOW_THRESH;
    this->e1000_hw.fc_pause_time = E1000_FC_PAUSE_TIME;
    this->e1000_hw.fc_send_xon = TRUE;
    this->tx_int_delay = 32;
    this->tx_abs_int_delay = 128;

    // Give priority to receive DMA, or be fair?
    this->e1000_hw.dma_fairness = TRUE;

    // I suspect that rx_int_delay is the delay to wait between packet
    // arrival before firing an interrupt to notify that packets have arrived.
    // And rx_abs_int_delay is the maximum amount of time to wait.
    this->rx_int_delay = 32;
    this->rx_abs_int_delay = 512;
    this->rx_csum = true;

    // So far, my experiments show that a throttle rate of 0 is best for
    // receive.  But using a throttle, such as 8000, improves send perf.
    this->int_throttle_rate = 8000;
    if( this->int_throttle_rate )
	// Copied from the Intel driver.
	this->int_throttle_rate = 1000000000/(this->int_throttle_rate * 256);

    if( this->e1000_hw.mac_type >= e1000_82543 ) {
	uint32_t status = E1000_READ_REG( &this->e1000_hw, STATUS );
	if( status & E1000_STATUS_TBIMODE )
	    this->e1000_hw.media_type = e1000_media_type_fiber;
	else
	    this->e1000_hw.media_type = e1000_media_type_copper;
    }
    else
	this->e1000_hw.media_type = e1000_media_type_fiber;

    this->e1000_hw.report_tx_early = (this->e1000_hw.mac_type < e1000_82543) ? FALSE:TRUE;
    this->e1000_hw.wait_autoneg_complete = FALSE;
    this->e1000_hw.tbi_compatibility_en = FALSE;
    this->e1000_hw.adaptive_ifs = TRUE;

    if( this->e1000_hw.media_type == e1000_media_type_copper ) {
	this->e1000_hw.mdix = AUTO_ALL_MODES;
	this->e1000_hw.disable_polarity_correction = FALSE;
    }
    else
	printf( "e1000[%d]: found an unexpected media type!\n", this->which );

    if( e1000_validate_eeprom_checksum(&this->e1000_hw) < 0 )
    {
	printf( "e1000[%d]: invalid eeprom checksum\n", this->which );
	return false;
    }
    e1000_read_mac_addr( &this->e1000_hw );
    nprintf( 1, "e1000[%d]: mac address %.6b\n", this->which, 
	    this->e1000_hw.mac_addr );
    lanaddress_set_seed( this->e1000_hw.mac_addr );

    e1000_read_part_num( &this->e1000_hw, &this->part_num );
    nprintf( 1, "e1000[%d]: part num: %x\n", this->which, this->part_num );

    e1000_get_bus_info( &this->e1000_hw );
    nprintf( 1, "e1000[%d]: bus type %d, speed: %d, width: %d\n",
	    this->which, this->e1000_hw.bus_type,
	    this->e1000_hw.bus_speed, this->e1000_hw.bus_width );

    this->e1000_hw.fc = this->e1000_hw.original_fc = e1000_fc_full;
    this->e1000_hw.autoneg = TRUE;
    this->e1000_hw.autoneg_advertised = AUTONEG_ADVERTISE_SPEED_DEFAULT;
//    this->e1000_hw.autoneg = 0;
//    this->e1000_hw.forced_speed_duplex = e1000_100_full;

    this->reset();
    return true;
}

void E1000Driver::reset()
{
    if( NetRxPacket::size > E1000_RXBUFFER_8192)
	E1000_WRITE_REG( &this->e1000_hw, PBA, E1000_JUMBO_PBA );
    else
	E1000_WRITE_REG( &this->e1000_hw, PBA, E1000_DEFAULT_PBA );

    this->e1000_hw.fc = this->e1000_hw.original_fc;
    e1000_reset_hw( &this->e1000_hw );
    if( this->e1000_hw.mac_type >= e1000_82544 )
	E1000_WRITE_REG( &this->e1000_hw, WUC, 0 );
    e1000_init_hw( &this->e1000_hw );
    e1000_reset_adaptive( &this->e1000_hw );
    e1000_phy_get_info( &this->e1000_hw, &this->phy_info );

    nprintf( 2, "e1000[%d]: TXDCTL = 0x%08x\n", this->which,
	    E1000_READ_REG(&this->e1000_hw, TXDCTL) );
    nprintf( 2, "e1000[%d]: RXDCTL = 0x%08x\n", this->which,
	    E1000_READ_REG(&this->e1000_hw, RXDCTL) );
}

void E1000Driver::enable_promiscuous( u32_t flag )
{
    u32_t rctl = E1000_READ_REG( &this->e1000_hw, RCTL );
    rctl |= flag;
    E1000_WRITE_REG( &this->e1000_hw, RCTL, rctl );
}

void E1000Driver::disable_promiscuous( u32_t flag )
{
    u32_t rctl = E1000_READ_REG( &this->e1000_hw, RCTL );
    rctl &= flag;
    E1000_WRITE_REG( &this->e1000_hw, RCTL, rctl );
}

void E1000Driver::configure_tx()
{
    ASSERT( (this->tx_ring.slots % 8) == 0 );
    ASSERT( this->tx_ring.slots >= min_ring_descriptors() );
    ASSERT( this->tx_ring.slots < max_ring_descriptors() );

    uint32_t tdba = virt_to_dma( &this->tx_ring.ring );
    ASSERT( (tdba % 16) == 0 );
    uint32_t tdlen = this->tx_ring.slots * sizeof(E1000TxDesc);
    ASSERT( (tdlen % 128) == 0 );
    uint32_t tctl, tipg;

    ASSERT( sizeof(word_t) == 4 );
    E1000_WRITE_REG( &this->e1000_hw, TDBAL, tdba );
    E1000_WRITE_REG( &this->e1000_hw, TDBAH, 0 );

    E1000_WRITE_REG( &this->e1000_hw, TDLEN, tdlen );

    /* Setup the HW Tx Head and Tail descriptor pointers */

    E1000_WRITE_REG( &this->e1000_hw, TDH, 0 );
    E1000_WRITE_REG( &this->e1000_hw, TDT, 0 );

    /* Set the default values for the Tx Inter Packet Gap timer */

    switch( this->e1000_hw.mac_type ) {
	case e1000_82542_rev2_0:
	case e1000_82542_rev2_1:
	    tipg = DEFAULT_82542_TIPG_IPGT;
	    tipg |= DEFAULT_82542_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
	    tipg |= DEFAULT_82542_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
	    break;
	default:
	    if( this->e1000_hw.media_type == e1000_media_type_fiber )
		tipg = DEFAULT_82543_TIPG_IPGT_FIBER;
	    else
		tipg = DEFAULT_82543_TIPG_IPGT_COPPER;
	    tipg |= DEFAULT_82543_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
	    tipg |= DEFAULT_82543_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
    }
    E1000_WRITE_REG( &this->e1000_hw, TIPG, tipg );

    /* Set the Tx Interrupt Delay register */

    E1000_WRITE_REG( &this->e1000_hw, TIDV, this->tx_int_delay );
    if( this->e1000_hw.mac_type >= e1000_82540 )
	E1000_WRITE_REG( &this->e1000_hw, TADV, this->tx_abs_int_delay );

    /* Program the Transmit Control Register */

    tctl = E1000_READ_REG( &this->e1000_hw, TCTL );

    tctl &= ~E1000_TCTL_CT;
    tctl |= E1000_TCTL_EN | E1000_TCTL_PSP |
	(E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);

    E1000_WRITE_REG( &this->e1000_hw, TCTL, tctl );

    e1000_config_collision_dist( &this->e1000_hw );

    /* Setup Transmit Descriptor Settings for this adapter */
    this->txd_cmd = E1000_TXD_CMD_IFCS | E1000_TXD_CMD_IDE;

    if( this->e1000_hw.report_tx_early == 1 )
	this->txd_cmd |= E1000_TXD_CMD_RS;
    else
	this->txd_cmd |= E1000_TXD_CMD_RPS;
}

void E1000Driver::configure_rx()
{
    ASSERT( (this->rx_ring.slots % 8) == 0 );
    ASSERT( this->rx_ring.slots >= min_ring_descriptors() );
    ASSERT( this->rx_ring.slots < max_ring_descriptors() );

    uint32_t rdba = virt_to_dma( &this->rx_ring.ring );
    ASSERT( (rdba % 16) == 0 );
    uint32_t rdlen = this->rx_ring.slots * sizeof(E1000RxDesc);
    ASSERT( (rdlen % 128) == 0 );
    uint32_t rctl;
    uint32_t rxcsum;

    /* make sure receives are disabled while setting up the descriptors */

    rctl = E1000_READ_REG(&this->e1000_hw, RCTL);
    E1000_WRITE_REG(&this->e1000_hw, RCTL, rctl & ~E1000_RCTL_EN);

    /* set the Receive Delay Timer Register */

    E1000_WRITE_REG(&this->e1000_hw, RDTR, this->rx_int_delay);

    if(this->e1000_hw.mac_type >= e1000_82540) {
	E1000_WRITE_REG(&this->e1000_hw, RADV, this->rx_abs_int_delay);
	E1000_WRITE_REG(&this->e1000_hw, ITR, this->int_throttle_rate);
    }

    /* Setup the Base and Length of the Rx Descriptor Ring */

    ASSERT( sizeof(word_t) == 4 );
    E1000_WRITE_REG(&this->e1000_hw, RDBAL, rdba);
    E1000_WRITE_REG(&this->e1000_hw, RDBAH, 0);

    E1000_WRITE_REG(&this->e1000_hw, RDLEN, rdlen);
    nprintf( 2, "e1000[%d]: added %d receive descriptors, ring size %d\n",
	    this->which, rx_ring.slots, rdlen );

    /* Setup the HW Rx Head and Tail Descriptor Pointers */
    E1000_WRITE_REG(&this->e1000_hw, RDH, 0);
    E1000_WRITE_REG(&this->e1000_hw, RDT, 0);

    /* Enable 82543 Receive Checksum Offload for TCP and UDP */
    if((this->e1000_hw.mac_type >= e1000_82543) &&
	    (this->rx_csum == TRUE)) {
	rxcsum = E1000_READ_REG(&this->e1000_hw, RXCSUM);
	rxcsum |= E1000_RXCSUM_TUOFL;
	E1000_WRITE_REG(&this->e1000_hw, RXCSUM, rxcsum);
    }

    /* Enable Receives */

    E1000_WRITE_REG(&this->e1000_hw, RCTL, rctl);
}

void E1000Driver::setup_rctl()
{
    uint32_t rctl;

    rctl = E1000_READ_REG(&this->e1000_hw, RCTL);

    rctl &= ~(3 << E1000_RCTL_MO_SHIFT);
    rctl &= ~E1000_RCTL_EN;

    rctl |= E1000_RCTL_BAM |
	E1000_RCTL_LBM_NO | E1000_RCTL_RDMTS_HALF |
	(this->e1000_hw.mc_filter_type << E1000_RCTL_MO_SHIFT);

    if(this->e1000_hw.tbi_compatibility_on == 1)
	rctl |= E1000_RCTL_SBP;
    else
	rctl &= ~E1000_RCTL_SBP;

    rctl &= ~(E1000_RCTL_SZ_4096);
    switch (NetRxPacket::size) {
	case KB(2):
	default:
	    rctl |= E1000_RCTL_SZ_2048;
	    rctl &= ~(E1000_RCTL_BSEX | E1000_RCTL_LPE);
	    break;
	case KB(4):
	    rctl |= E1000_RCTL_SZ_4096 | E1000_RCTL_BSEX | E1000_RCTL_LPE;
	    break;
	case KB(8):
	    rctl |= E1000_RCTL_SZ_8192 | E1000_RCTL_BSEX | E1000_RCTL_LPE;
	    break;
	case KB(16):
	    rctl |= E1000_RCTL_SZ_16384 | E1000_RCTL_BSEX | E1000_RCTL_LPE;
	    break;
    }

    E1000_WRITE_REG(&this->e1000_hw, RCTL, rctl);
}

bool E1000Driver::setup_tx_resources()
{
    this->tx_ring.free = 0;
    this->tx_ring.dirty = 0;

    for( word_t i = 0; i < this->tx_ring.slots; i++ )
	this->tx_shadow[i] = NULL;

    return true;
}

bool E1000Driver::setup_rx_resources()
{
    rx_ring.free = 0;
    rx_ring.dirty = 0;

    return true;
}

void E1000Driver::enable_rx_copy_buffers()
{
    nprintf( 2, "e1000[%d]: enabling receive copy buffers\n", this->which );

    // Disable receives.
    uint32_t rctl = E1000_READ_REG(&this->e1000_hw, RCTL);
    E1000_WRITE_REG(&this->e1000_hw, RCTL, rctl & ~E1000_RCTL_EN);

    // Calculate the descriptor buffers.  All packets are delivered to
    // static buffers, and then copied to client buffers.  We align the
    // receive buffers to 2-bytes,  to align the data payload to 4-bytes
    // (after the 14-byte ethernet frame header).
    for( u32_t i = 0; i < rx_ring.slots; i++ )
    {
	E1000RxDesc *rx_desc = &rx_ring.ring[i];
	if( rx_desc->buffer_low == 0 )  {
	    NetRxPacket *pkt = get_pkt_pool()->pop();
	    ASSERT( pkt );
	    rx_desc->buffer_low = cpu_to_le32( virt_to_dma(&pkt->raw) );
	}
	rx_desc->release_to_device();
    }

    rx_ring.free = 0;
    rx_ring.dirty = 0;

    // Expose the buffers to the NIC.
    E1000_WRITE_REG(&this->e1000_hw, RDH, 0);
    E1000_WRITE_REG(&this->e1000_hw, RDT, rx_ring.slots-1);
    // Enable receives.
    E1000_WRITE_REG(&this->e1000_hw, RCTL, rctl | E1000_RCTL_EN );
}

void E1000Driver::irq_enable()
{
    E1000_WRITE_REG( &this->e1000_hw, IMS, IMS_ENABLE_MASK );
    E1000_WRITE_FLUSH( &this->e1000_hw );
}

void E1000Driver::irq_disable()
{
    E1000_WRITE_REG( &this->e1000_hw, IMC, ~UL(0) );
    E1000_WRITE_FLUSH( &this->e1000_hw );
}

bool E1000Driver::open()
{
    if( !this->setup_tx_resources() )
	return false;
    if( !this->setup_rx_resources() )
	return false;
    this->configure_tx();
    this->setup_rctl();
    this->configure_rx();
    this->enable_rx_copy_buffers();
    this->irq_enable();

    return true;
}

bool E1000Driver::close()
{
    // TODO: release physical memory pages used by the descriptors.
    PANIC( "UNIMPLEMENTED" );
    this->irq_disable();
    this->reset();
    return true;
}

void E1000Driver::irq_handler( L4_ThreadId_t active_tid )
{
    uint32_t icr;
    int count = 0;
    static int max = 0;

    // For SMP, another thread may attempt to lock the device.  If it fails,
    // it aborts, but sets the event_pending flag.  Because we hold the
    // lock, it is our responsibility to attempt to handle the event that
    // the other thread wanted dispatched.  We use a loop which executes
    // as long as event_pending is true.
    do
    {
	SMP_OP( irq_pending = true );
	if( !this->lock_device() )
	    return;
	SMP_OP( irq_pending = false );

	count++;
	if( count > max )
	{
	    max = count;
	    nprintf( 1, "e1000[%d]: consecutive locks: %d\n", this->which, max );
	}

	STAT_OP( get_stats(this)->delivered_interrupts++ );

	this->server_shared->x.irq_status = 0;
	this->server_shared->x.irq_pending = true;

	while( (icr = E1000_READ_REG(&this->e1000_hw, ICR)) )
	{
	    STAT_OP( get_stats(this)->interrupts++ );

	    if( icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC) ) {
		nprintf( 1, "e1000[%d]: link status change\n", this->which );
		this->link_status_report();
	    }
	    if( icr & E1000_ICR_RXO )
		nprintf( 2, "e1000[%d]: rx overrun\n", this->which );
#if 0
	    if( icr & E1000_ICR_TXDW )
		nprintf( 3, "e1000[%d]: transmit desc written back\n", this->which);
	    if( icr & E1000_ICR_TXQE )
		nprintf( 3, "e1000[%d]: transmit queue empty\n", this->which );
	    if( icr & E1000_ICR_RXDMT0 )
		nprintf( 2, "e1000[%d]: rx desc min. threshold\n", this->which );
	    if( icr & E1000_ICR_RXT0 )
		nprintf( 3, "e1000[%d]: rx timer intr (ring 0)\n", this->which );
	    if( icr & E1000_ICR_MDAC )
		nprintf( 3, "e1000[%d]: MDIO access complete\n", this->which );
	    if( icr & E1000_ICR_RXCFG )
		nprintf( 3, "e1000[%d]: RX /c/ ordered set\n", this->which );
	    if( icr & E1000_ICR_GPI_EN0 )
		nprintf( 3, "e1000[%d]: GP int 0\n", this->which );
	    if( icr & E1000_ICR_GPI_EN1 )
		nprintf( 3, "e1000[%d]: GP int 1\n", this->which );
	    if( icr & E1000_ICR_GPI_EN2 )
		nprintf( 3, "e1000[%d]: GP int 2\n", this->which );
	    if( icr & E1000_ICR_GPI_EN3 )
		nprintf( 3, "e1000[%d]: GP int 3\n", this->which );
#endif

	    this->clean_tx_ring();
	    this->run_send_queue();
	    this->clean_copy_rx();
	}

	if( this->tx_ring.is_empty() )
	    this->server_shared->x.irq_pending = false;

    } while( this->unlock_device() );

    this->deliver_pending_client_pkts();
    this->deliver_pending_client_msgs( active_tid );
}

void E1000Driver::send_handler( L4_ThreadId_t active_tid )
{
#if defined(CONFIG_SMP)
    if( irq_pending )
    {
	// Hmmm, we missed an event due to a lock.  In the worst case, the
	// missed event was an interrupt, so we switch to the interrupt
	// handler.
	this->irq_handler( active_tid );
	return;
    }
#endif

    if( this->lock_device() )
    {
	this->server_shared->x.irq_pending = true;

	STAT_OP( get_stats(this)->send_requests++ );
	this->run_send_queue();
//	this->clean_copy_rx();
//	this->deliver_pending_client_pkts();
//	this->clean_tx_ring();

	if( this->tx_ring.is_empty() )
	    this->server_shared->x.irq_pending = false;

	if( this->unlock_device() )
	    this->irq_handler( active_tid ); // Clean-up after missed events due to our lock
    }
}

void E1000Driver::link_status_report()
{
    NetServerShared *status = get_server_shared( this->which );

    // Query the device for link status.
    this->e1000_hw.get_link_status = TRUE;
    e1000_check_for_link( &this->e1000_hw );

    // Print to the debug console a summary of the link status change.
    if( E1000_READ_REG(&this->e1000_hw, STATUS) & E1000_STATUS_LU )
    {
	uint16_t speed, duplex;
	e1000_get_speed_and_duplex( &this->e1000_hw, &speed, &duplex );

	status->full_duplex_status(duplex == FULL_DUPLEX);
	status->link_speed_status( speed );
	status->set_link_valid();

	printf( "e1000[%d]: link is up, %d Mbps at %s\n",
		this->which, speed,
		duplex == FULL_DUPLEX ? "full duplex" : "half duplex" );
    }
    else {
	status->clr_link_valid();
	printf( "e1000[%d]: link failure\n", this->which );
    }

    // Notify each client about the link status change.
    for( NetIface *iface = this->iface_list; iface; iface = iface->next)
	iface->report_link_status_change();
}

void E1000Driver::enable_lan_addr( NetIface *iface )
{
    // Find the first available slot in the LAN address bitmap by finding
    // the first zero bit.  We use the corresponding slot in the RAR table.
    word_t slot = lsb(this->lan_addr_bitmap);

    if( (slot >= E1000_RAR_ENTRIES) || (this->lan_addr_bitmap == ~0UL) )
    {
	// We have too many LAN addresses, so enable promiscuous mode and
	// accept all inbound packets.
	this->enable_promiscuous_lan_addr();
	slot = E1000_RAR_ENTRIES;
    }
    else
    {
	this->lan_addr_bitmap |= (1 << slot);
	LanAddr mac_addr;
	iface->get_lan_addr( mac_addr.raw );
	e1000_rar_set( &this->e1000_hw, mac_addr.raw, slot );
    }

    iface->set_lan_addr_slot( slot );
}

void E1000Driver::remove_iface( NetIface *iface )
{
    NetIface *last, *search;

    if( this->iface_list == NULL )
    {
	// TODO: Disable the receive buffers if no interfaces.
    }

    // The list is maintained in reverse order of insertions.
    if( this->iface_list == iface )
	this->iface_list = iface->next;
    else if( this->iface_list == NULL )
	return;	// iface not found
    else
    {
	// Hunt for the iface
	last = this->iface_list;
	search = this->iface_list->next;

	while( search && (search != iface) ) {
	    last = search;
	    search = search->next;
	}
	if( !search )
	    return;	// iface not found

	// Remove the iface from the linked list.
	last->next = iface->next;
	iface->next = NULL;
    }

    this->iface_cnt--;

    // Free the LAN address entry in the hardware lookup table.
    if( iface->get_lan_addr_slot() < E1000_RAR_ENTRIES )
	this->lan_addr_bitmap &= ~(1 << iface->get_lan_addr_slot());
    // TODO: figure out when we can leave promiscuous mode if we can now
    // fit all LAN addresses into the hardware lookup table.
}

bool E1000Driver::tx_inject_csum( NetClientDesc *client_desc )
{
    STAT_OP( get_stats(this)->csum++ );
    word_t csum_start = client_desc->csum_start_offset();
    word_t csum_offset = client_desc->csum_store_offset();

#if defined(CSUM_OPTIMIZATION)
    if( (csum_start == this->last_csum_start) && (csum_offset == this->last_csum_offset) )
	return false;	// We can use our last checksum configuration.
#endif

    last_csum_start = csum_start;
    last_csum_offset = csum_offset;

    ASSERT( tx_shadow[tx_ring.free] == NULL );
    tx_shadow[tx_ring.free] = get_iface_anon();

    struct e1000_context_desc *context_desc;
    context_desc = (e1000_context_desc *)&tx_ring.ring[ tx_ring.free ];
    context_desc->upper_setup.tcp_fields.tucss = csum_start;
    context_desc->upper_setup.tcp_fields.tucso = csum_offset;
    context_desc->upper_setup.tcp_fields.tucse = 0;
    context_desc->tcp_seg_setup.data = 0;
    context_desc->cmd_and_length =
	cpu_to_le32(this->txd_cmd | E1000_TXD_CMD_DEXT);

    tx_ring.inc_free();
    return true;
}

void E1000Driver::run_send_queue()
{
    NetIface *iface;
    NetClientDesc *client_desc;
    u32_t sent_fragments, tot = 0;

    nprintf( 3, "e1000[%d]: run_send_queue()\n", this->which);

    do {
	sent_fragments = 0;

	for( iface = this->iface_list; iface; iface = iface->next )
	{
	    client_desc = iface->get_next_tx_pkt();
	    if( client_desc == NULL )
		continue;
	    u32_t sent = this->tx_packet( iface, client_desc );
	    if( sent == 0 ) {
		// Nothing to send.
		iface->report_empty_queue();
		continue;
	    }
	    sent_fragments += sent;
	    tot += sent;
	}

    } while( sent_fragments > 0 );

    if( tot )
    {
	// TODO: Should we tell the adapter to send the packets here, after
	// we insert them into the queue, or after each insertion?
	// Bus bandwidth consumed for each register write.
	E1000_WRITE_REG( &this->e1000_hw, TDT, tx_ring.free );
	STAT_OP( get_stats(this)->pkts_sent += tot );
    }
}

int E1000Driver::tx_packet( NetIface *iface, NetClientDesc *client_desc )
{
    u32_t txd_upper, txd_lower;
    E1000TxDesc *tx_desc = NULL;
    u16_t sent, i, frag, fragments, descriptor_cnt;
    u16_t tx_avail;

    // How many descriptors do we need?  How many fragments do we have?
    sent = 0;
    if( !client_desc->is_tx_dev_owned() )
	return sent;

    fragments = 1;
    NetClientDesc *fragment = client_desc;
    while( fragment->are_more() ) {
	fragments++;
	fragment = iface->client_to_tx_ring( fragment->get_client_link() );
	if( !fragment->is_tx_dev_owned() ) {
	    nprintf( 1, "e1000[%d]: incomplete fragments\n", this->which );
	    return sent;	// Abort - the descriptors are incomplete.
	}
	if( fragment == client_desc ) {
	    nprintf( 1, "e1000[%d]: fragments wrap-around\n", this->which );
	    return sent;	// Abort - wrap-around.
	}
    }
    descriptor_cnt = fragments;
    if( client_desc->has_csum() )
	descriptor_cnt++;
    STAT_OP( get_stats(this)->descriptor_cnt += descriptor_cnt );

    // Do we need to clean some descriptors?
    tx_avail = this->tx_ring.available();
    if( descriptor_cnt > tx_avail )
    {
	STAT_OP( get_stats(this)->need_tx_clean++ );
	tx_avail += this->clean_tx_ring();
	ASSERT(client_desc->is_tx_dev_owned());
	if( descriptor_cnt > tx_avail )
	{
	    STAT_OP( get_stats(this)->nic_tx_full++ );
	    nprintf( 1, "e1000[%d]: tx descriptor full, %d > %d, %d/%d\n",
		    this->which, descriptor_cnt, this->tx_ring.available(),
		    tx_ring.free, tx_ring.dirty);
	    return sent;
	}
    }

    // We will send the packet fragments.
    nprintf( 4, "e1000[%d]: queuing packet %p, buffer %p, csum? %d, len %d\n", 
	    this->which, client_desc, client_desc->buffer, 
	    client_desc->has_csum(), client_desc->get_len() );

    txd_upper = 0;
    txd_lower = this->txd_cmd;

    iface->update_tx_dirty_start( client_desc );

    if( client_desc->has_csum() )
    {
	if( this->tx_inject_csum(client_desc) )
	    sent++;
	txd_lower |= E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
	txd_upper |= E1000_TXD_POPTS_TXSM << 8;
    }

    i = tx_ring.free;
    for( frag = 0; frag < fragments; frag++ )
    {
	// Update the shadow ring.
	ASSERT( tx_shadow[i] == NULL );
	tx_shadow[i] = iface;

	// Update the E1000 descriptor ring.
	tx_desc = &tx_ring.ring[i];
	ASSERT( tx_desc->upper.data == 0 );
	tx_desc->buffer_high = 0;  // Important!  Otherwise malfunction.
	tx_desc->buffer_low =
	    cpu_to_le32( iface->client_phys_to_dma(client_desc->get_client_buffer()) );
	tx_desc->lower.data = cpu_to_le32( txd_lower | client_desc->get_len());
	tx_desc->upper.data = cpu_to_le32( txd_upper );

	// Increment counters.
	iface->set_next_tx_pkt( client_desc );
	client_desc = iface->get_next_tx_pkt();
	i = (i + 1) % tx_ring.slots;
	sent++;
    }
    tx_ring.free = i;

    // End of fragment sequence.  Update the last used descriptor.
    if( tx_desc )
    	tx_desc->lower.data |= cpu_to_le32( E1000_TXD_CMD_EOP );
    // Tell the device to send the packets.
//    E1000_WRITE_REG( &this->e1000_hw, TDT, tx_ring.free );

    return sent;
}


u16_t E1000Driver::clean_tx_ring()
{
    E1000TxDesc *tx_desc;
    NetIface *iface;
    NetClientDesc *client_desc;
    int i;
    u16_t cleaned = 0;
    bool do_irq;

    i = tx_ring.dirty;
    tx_desc = &tx_ring.ring[i];

    while( tx_desc->upper.data & cpu_to_le32(E1000_TXD_STAT_DD) )
    {
	iface = tx_shadow[i];
	if( !iface )
	{
	    nprintf( 1, "e1000[%d]: premature send descriptor cleaning\n",
		    this->which );
	    break;
	}

	// Some descriptor types may not have an iface!
	if( iface != get_iface_anon() )
	{
	    client_desc = iface->get_next_tx_dirty();
	    // The corresponding buffer should be the next one available for
	    // cleaning ...
	    ASSERT( tx_desc->buffer_low != 0 );
	    ASSERT( cpu_to_le32(iface->client_phys_to_dma(client_desc->get_client_buffer())) == tx_desc->buffer_low );
	    ASSERT( client_desc->is_tx_dev_owned() );
	    
	    do_irq = client_desc->has_irq_requested();

	    // Release the buffer/descriptor from the driver, to Linux.
	    client_desc->set_ok();
	    client_desc->release_to_client();
	    iface->inc_tx_dirty();

	    // Notify the client as necessary.
	    iface->report_desc_released( do_irq );
	}

	// Release the buffer/descriptor from the network card.
	tx_shadow[i] = NULL;
	tx_desc->buffer_low = NULL;
	tx_desc->upper.data = 0;

	i = (i + 1) % tx_ring.slots;
	tx_desc = &tx_ring.ring[i];
	cleaned++;
    }

    nprintf( 4, "e1000[%d]: cleaned %d tx descriptors\n", this->which,
	    cleaned);
    tx_ring.dirty = i;

    return cleaned;
}

u16_t E1000Driver::clean_copy_rx()
{
    E1000RxDesc *rx_desc;
    int i;
    NetIface *iface;
    u16_t cleaned = 0;

    // Scan the receive descriptor ring for ready packets.
    i = rx_ring.dirty;
    rx_desc = &rx_ring.ring[i];

    while( rx_desc->status )
    {
	u16_t pkt_len = le16_to_cpu(rx_desc->length);
	NetRxPacket *pkt = dma_to_virt<NetRxPacket*>( le32_to_cpu(rx_desc->buffer_low) - offsetof(NetRxPacket,raw) );
	LanAddr *lan_addr = &pkt->lan_addr;

	bool hw_has_csum =
	    !(rx_desc->status & E1000_RXD_STAT_IXSM)  &&
	     (rx_desc->status & E1000_RXD_STAT_TCPCS) &&
	    !(rx_desc->errors & E1000_RXD_ERR_TCPE);

	if( (rx_desc->status & E1000_RXD_STAT_EOP) &&
		!(rx_desc->errors & E1000_RXD_ERR_FRAME_ERR_MASK) )
	{
	    // The packet must fit within a single buffer, and
	    // must have no delivery errors.
	    pkt_len -= ETHERNET_FCS_SIZE;

	    pkt->pkt_len = pkt_len;
	    pkt->hw_csum = rx_desc->csum;
	    pkt->valid_hw_csum = hw_has_csum;

	    if( lan_addr->is_dst_broadcast() )
	    {
		STAT_OP( get_stats(this)->broadcast++ );
		nprintf( 4, "e1000[%d]: broadcast packet, len %d\n",
			this->which, pkt_len );
		for( iface = this->iface_list; iface; iface = iface->next )
		    iface->queue_copy_pkt( pkt );
	    }
	    else
	    {
		STAT_OP( get_stats(this)->unicast++ );
		nprintf( 4, "e1000[%d]: unicast packet, len %d\n",
			this->which, pkt_len );

		iface = get_iface_map()->get_iface( lan_addr->get_handle() );
		if( iface )
		    iface->queue_copy_pkt( pkt );
	    }
	}
	else
	    nprintf( 1,
		    "e1000[%d]: dropped packet, len %d, status %x, errors %x\n",
		    this->which, pkt_len, rx_desc->status, rx_desc->errors );

	if( !pkt->is_free() ) {
	    // We have to allocate a new packet here from the packet pool.
	    pkt = get_pkt_pool()->pop();
	    ASSERT( pkt );
	    rx_desc->buffer_low = cpu_to_le32( virt_to_dma(&pkt->raw) );
	    rx_desc->buffer_high = 0;
	}

	rx_desc->release_to_device();

	// TODO: should we rate limit the update to RDT??  The linux driver
	// rate limits, presumably to reduce bus traffic, but I have found
	// that rate limiting can also reduce performance.
	// This needs more study.
	if( !(i % 4) )
	    E1000_WRITE_REG( &this->e1000_hw, RDT, i );

	// Increment counters.
	i = (i + 1) % rx_ring.slots;
	rx_desc = &rx_ring.ring[i];
	cleaned++;
    }
    rx_ring.dirty = i;

#if defined(NET_STATS)
    if( cleaned > get_stats(this)->pkts_per_int )
	get_stats(this)->pkts_per_int = cleaned;
#endif

    return cleaned;
}

/****************************************************************************/

extern "C" void
e1000_pci_clear_mwi( struct e1000_hw *hw )
{
    E1000Driver *driver = (E1000Driver *)hw->back;
    driver->pci_config.clear_mwi();
}

extern "C" void
e1000_pci_set_mwi( struct e1000_hw *hw )
{
    E1000Driver *driver = (E1000Driver *)hw->back;
    driver->pci_config.set_mwi();
}

extern "C" void
e1000_read_pci_cfg(struct e1000_hw *hw, uint32_t reg, uint16_t *value)
{
    E1000Driver *driver = (E1000Driver *)hw->back;
    *value = driver->pci_config.read_config16( reg );
}

extern "C" void
e1000_write_pci_cfg(struct e1000_hw *hw, uint32_t reg, uint16_t *value)
{
    E1000Driver *driver = (E1000Driver *)hw->back;
    driver->pci_config.write_config16( reg, *value );
}

extern "C" uint32_t
e1000_io_read(struct e1000_hw *hw, uint32_t port)
{
    return in_u32( port );
}

extern "C" void
e1000_io_write(struct e1000_hw *hw, uint32_t port, uint32_t value)
{
    out_u32( port, value );
}

/****************************************************************************/
