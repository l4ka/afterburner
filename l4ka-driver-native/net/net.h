#ifndef __NET__NET_H__
#define __NET__NET_H__

#include <common/resourcemon.h>
#include <common/ia32/page.h>
#include <common/ia32/bitops.h>

//#define CONFIG_SMP
//#define DIRECT_DMA

#if !defined(DIRECT_DMA)
#define CONFIG_PACKET_COPY
#endif

#include <l4/types.h>
#include <l4/kdebug.h>

#include <L4VMnet_idl_server.h>

#include <common/console.h>
#include <common/debug.h>

#define LAN_ADDRESS_BYTES	6
#define MAX_FRAME_SIZE		1522	// 802.1Q (VLAN support)

#if defined(cfg_optimize)
# define debug_net	2
#else
extern int debug_net;
#endif

#define nprintf(n,a...) do{ if(debug_net >= (n)) printf(a); } while(0)

#if defined(NET_STATS)
#define STAT_OP(a)	a
#else
#define STAT_OP(a)
#endif

class E1000Driver;

typedef enum {ISR_TXOK=6, ISR_TXDESC=7, ISR_TXERR=8, ISR_TXIDLE=9} dp83820_isr_e;
typedef enum {ISR=0x10/4, TXDP=0x20/4, IMR=0x14/4, IER=0x18/4} dp83820_regs_e;

typedef u16_t net_handle_t;

#if !defined(__i386__)
# error "LanAddr is little-endian use only."
#endif

class LanAddr {
public:
    union {
	struct {
	    // For 2-byte aligned addresses, in the ethernet frame header.
	    u16_t lsb;
	    u32_t msb __attribute__ ((packed));
	} dst;

	struct {
	    // For 4-byte aligned addresses, in the ethernet frame header.
	    u32_t lsb __attribute__ ((packed));
	    u16_t msb;
	} src;

	struct {
	    u8_t group : 1;  // multicast if set
	    u8_t local : 1;	// local if set
	    u8_t : 6;
	} status;

	u16_t halfwords[ LAN_ADDRESS_BYTES/2 ];
	u8_t raw[ LAN_ADDRESS_BYTES ];

    };

    void set_local()   { this->status.local = 1; }
    void set_unicast() { this->status.group = 0; }

    void set_handle( u16_t handle ) { this->halfwords[2] = handle; }
    u16_t get_handle() { return this->halfwords[2]; }

    bool is_local() { return (this->status.local == 1); }
    bool is_group() { return (this->status.group == 1); }
    bool is_dst_broadcast() 
    { 
	return (this->dst.lsb == 0xffff) && 
	       (this->dst.msb == 0xffffffff);
    }

};


class LanHeader {
public:
    LanAddr dst;
    LanAddr src;
    union {
	u16_t type;
	u16_t length;
    };
    unsigned char data[0];
};


class NetRxPacket {
public:
    static const word_t size = KB(2);	// Certain sizes are demanded by E1000

    u16_t pkt_len;
    u16_t hw_csum;
    u8_t  valid_hw_csum;
    u8_t  refcnt;		// 2-byte offset, to achieve alignment
    union {
	u8_t raw[size];
	LanAddr lan_addr;
	struct {
	    u16_t skip;
	    NetRxPacket *next;	// Pointer to the next free packet.
	} free;
    };

    NetRxPacket() { refcnt = 0; }
    void inc() { refcnt++; }
    void dec() { refcnt--; }
    bool is_free() { return 0 == refcnt; }
};

class NetRxPacketPool
{
public:
    static const word_t count = 1024;

    NetRxPacket pool[count];
    NetRxPacket *head;

    NetRxPacketPool() { reset(); }

    void reset()
    {
	// Create ring of available packets.
	for( word_t i = 0; i < count-1; i++ )
	    pool[i].free.next = &pool[i+1];
	pool[count-1].free.next = NULL;

	head = &pool[0];
    }

    NetRxPacket *pop()
    {
	if( !head )
	    return NULL;
	NetRxPacket *pkt = head;
	head = head->free.next;
	return pkt;
    }

    void push( NetRxPacket *pkt )
    {
	pkt->free.next = head;
	head = pkt;
    }
};

class NetClientDesc : public IVMnet_dp83820_descriptor_t
{
public:
    bool is_tx_dev_owned() { return cmd_status.tx.own; }
    bool is_queued() { return cmd_status.tx.ok; }
    bool has_irq_requested() { return cmd_status.tx.intr; }
    bool are_more() { return cmd_status.tx.more; }

    void set_ok() { cmd_status.tx.ok = 1; }
    void release_to_client() { cmd_status.tx.own = 0; }

    word_t get_client_link() { return link; }
    word_t get_len() { return buffer_len; }

    word_t get_client_buffer() { return buffer; }

    bool has_csum() { return extended_status.udp_pkt || extended_status.tcp_pkt || extended_status.ip_pkt; }
    word_t csum_start_offset() { return 34; }
    word_t csum_store_offset()
    {
	if( extended_status.udp_pkt )
	    return 40;
	else if( extended_status.tcp_pkt )
	    return 50;
	else if( extended_status.ip_pkt )
	    PANIC( "IP packet checksum not supported" );
    }
};

template <const word_t ring_slots, typename T>
class NetRing
{
public:
    static const word_t slots = ring_slots;
    static const word_t size = slots * sizeof(T);

    T ring[ slots ];
    word_t free;
    word_t dirty;

    void init();

    word_t pending() { return (free + slots - dirty) % slots; }
    word_t available() { return (dirty + slots - (free + 1)) % slots; }
    bool is_full() { return available() == 0; }
    bool is_empty() { return dirty == free; }

    void inc_free() { free = (free + 1) % slots; }
    void inc_dirty() { dirty = (dirty + 1) % slots; }
};

class NetClientShared {
public:
    union {
	IVMnet_client_shared_t x;
	word_t raw[ KB(8)/sizeof(word_t) ];
    };

    void clear() {
	for( word_t i = 0; i < sizeof(raw)/sizeof(raw[0]); i++ )
	    raw[i] = 0;
    }
};

class NetServerShared {
public:
    union {
	IVMnet_server_shared_t x;
	u8_t raw[PAGE_SIZE];
    };

    NetServerShared()
	{ x.irq_status = 0; x.irq_pending = 0; }

    void set_link_valid() {}
    void clr_link_valid() {}

    void full_duplex_status( bool status ) {}
    void link_speed_status( u16_t speed ) {}
};

class NetIface 
{
public:
    NetIface *next;  // Linked list associated with a specific device driver.

    space_info_t client_space;

    static const word_t rx_count = 256;
    NetRing<rx_count, NetRxPacket *> rx_ring;

    struct {
	L4_Fpage_t area;
	word_t client_phys;
	word_t ring_start;
	NetClientDesc *dirty;
	word_t ring_cnt;
    } dp83820;

protected:
    LanAddr addr;
    word_t lan_addr_slot; 
    E1000Driver *driver;
    NetClientShared *shared;
    bool active;
    bool promiscuous;
    bool pending_irq;

    bool send_irq( L4_ThreadId_t active_tid );

public:
    NetIface() { active = false; driver = NULL; shared = NULL; }

    void init( net_handle_t handle, E1000Driver *driver, NetClientShared *client_shared );
    void queue_copy_pkt( NetRxPacket *pkt )
    {
	if( rx_ring.is_full() )
	    printf( "Dropped packet!\n" );
	else {
	    ASSERT( pkt );
	    pkt->inc();
	    rx_ring.ring[ rx_ring.free ] = pkt;
	    rx_ring.inc_free();
	}
    }
    void activate();
    void deactivate();

public:
    net_handle_t get_handle() { return this->addr.get_handle(); }

    void flush_pkt_queue();
    bool xmit_pkts_to_client( int receiver );

    word_t rx_pending() { return rx_ring.pending(); }

    void report_link_status_change()
    {
    }

    void schedule_irq()
    {
	// We are higher priority than the client, so no worries about
	// race conditions.
	if( !shared->x.client_irq_pending
		&& (shared->x.dp83820_regs[ISR] & shared->x.dp83820_regs[IMR])
		&& shared->x.dp83820_regs[IER] )
	    this->pending_irq = true;
    }

    void report_desc_released( bool do_desc_irq )
    {
	bit_set( ISR_TXOK, shared->x.dp83820_regs[ISR] );
	if( do_desc_irq )
	    bit_set( ISR_TXDESC, shared->x.dp83820_regs[ISR] );
	//this->schedule_irq();
    }

    void report_empty_queue()
    {
	bit_set( ISR_TXIDLE, shared->x.dp83820_regs[ISR] );
	//this->schedule_irq();
    }

    void deliver_pending_msg( L4_ThreadId_t active_tid )
    {
	if( this->pending_irq )
	{
	    this->send_irq( active_tid );
	    this->pending_irq = false;
	}
    }

    bool want_pkt( LanAddr *dst )
    {
	return this->is_promiscuous() || 
	       this->dst_lanaddr_is_me( dst );
    }

    bool dst_lanaddr_is_me( LanAddr *d )
    {
	return (this->addr.dst.lsb == d->dst.lsb) &&
	       (this->addr.dst.msb == d->dst.msb);
    }

    word_t client_phys_to_dma( word_t client_phys )
    {
	ASSERT( client_phys < client_space.bus_size );
	return client_space.bus_start + client_phys;
    }
    word_t dma_to_client_phys( word_t dma )
    { 
	word_t phys = dma - client_space.bus_start;
	ASSERT( phys < client_space.bus_size );
	return phys;
    }

    NetClientDesc *client_to_tx_ring( word_t client_paddr )
	{ return (NetClientDesc *)(client_paddr - dp83820.client_phys + dp83820.ring_start); }

    NetClientDesc *get_next_tx_pkt()
    {
	if( shared->x.dp83820_regs[TXDP] == 0 )
	    return NULL;
	else
	    return client_to_tx_ring( shared->x.dp83820_regs[TXDP] );
    }
    void set_next_tx_pkt( NetClientDesc *current )
	{ shared->x.dp83820_regs[TXDP] = current->get_client_link(); }

    NetClientDesc *get_next_tx_dirty()
	{ return dp83820.dirty; }

    void inc_tx_dirty()
    {
	dp83820.dirty = client_to_tx_ring( dp83820.dirty->get_client_link() );
    }
    void update_tx_dirty_start( NetClientDesc *pkt )
    {
	if( dp83820.dirty == NULL )
	    dp83820.dirty = pkt;
    }

    NetClientShared *get_client_shared() { return this->shared; }

    E1000Driver *get_driver() { return this->driver; }
    bool is_promiscuous() { return this->promiscuous; }
    bool is_active() { return this->active; }

    void set_lan_addr( const u8_t mac_addr[sizeof(LanAddr)] )
    {
	for( unsigned i = 0; i < sizeof(LanAddr); i++ )
	    this->addr.raw[i] = mac_addr[i];
    }
    void get_lan_addr( u8_t mac_addr[sizeof(LanAddr)] )
    {
	for( unsigned i = 0; i < sizeof(LanAddr); i++ )
	    mac_addr[i] = this->addr.raw[i];
    }

    void set_lan_addr_slot( word_t slot )
    {
	this->lan_addr_slot = slot;
    }
    word_t get_lan_addr_slot()
    {
	return this->lan_addr_slot;
    }
};

extern inline NetIface *get_iface_anon()
{
    return (NetIface *)-1;
}

class NetIfaceMap
{
protected:
    static const net_handle_t size = cfg_max_net_clients;
    NetIface array[size];

public:
    NetIface *get_iface( net_handle_t index )
    {
	if( index < this->size )
	    return &this->array[ index ];
	return NULL;
    }
};

INLINE NetIfaceMap *get_iface_map()
{
    extern NetIfaceMap net_iface_map;
    return &net_iface_map;
}

INLINE NetRxPacketPool *get_pkt_pool()
{
    extern NetRxPacketPool pkt_pool;
    return &pkt_pool;
}

INLINE NetServerShared *get_server_shared( int dev_no )
{
    extern NetServerShared device_server_status[];
    return &device_server_status[ dev_no ];
}

INLINE NetClientShared *get_client_shared( net_handle_t handle )
{
    extern NetClientShared client_shared_regions[];
    return &client_shared_regions[ handle ];
}

#endif /* __NET__NET_H__ */
