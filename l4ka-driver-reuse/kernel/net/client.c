/*********************************************************************
 *
 * Copyright (C) 2004-2006,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/net/client.c
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
 ********************************************************************/

#include <l4/kip.h>

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>

#include <asm/io.h>

#include <glue/thread.h>
#include <glue/bottomhalf.h>
#include <glue/vmirq.h>
#include <glue/vmmemory.h>
#include <glue/wedge.h>


#include "client.h"

#if defined(CONFIG_X86_IO_APIC)
#include <acpi/acpi.h>
#include <linux/acpi.h>
#endif

#undef PREFIX
#define PREFIX "L4VMnet client: "

int L4VMnet_irq = 0;
MODULE_PARM( L4VMnet_irq, "i" );

static L4VMnet_client_adapter_t *L4VMnet_adapter_list = NULL;

static irqreturn_t L4VMnet_irq_handler( int irq, void *data, struct pt_regs *regs );

static int L4VMnet_irq_pending( void *data );
static void L4VMnet_flush_tasklet_handler( unsigned long unused );

DECLARE_TASKLET( L4VMnet_flush_tasklet, L4VMnet_flush_tasklet_handler, 0 );

/*
 * Initialization:
 *   TODO: how do we determine thread ID of the server?
 *   1.  Allocate an interrupt from Linux.
 *   2.  Register interrupt with server (put this on the shared page).
 *   3.  Allocate a descriptor ring for incoming packets.  The descriptor
 *       ring holds both the empty+waiting buffers and the newly arrived
 *       packets.  Visible only to the client.
 *   4.  Allocate a descriptor ring for outgoing packets.  The server must
 *       provide the memory region for the outgoing packet descriptors
 *       (for trust reasons).  Thus the client allocates a virtual
 *       memory region from Linux, and then receives a memory mapping
 *       into that region from the server.
 *   5.  Register some shared status words with the server.  One status
 *       word will be writeable by the server, to indicate virtual
 *       interrupt status.
 *   6.  Tell the server the thread ID of the inbound packet thread.
 *   7.  Ask the server for the feature list, such as hardware checksum,
 *       and hardware fragment support.
 *   8.  Generate a virtual LAN address.
 *
 * Sending packets:
 *   1.  Physical address of outbound fragments are added to a descriptor ring.
 *   2.  Send IPC to server, to inform that outbound packets are available.
 *       TODO: determine mechanism for IPC mitigation (for example, we know
 *       that a network driver will wake at some point to clean its old send
 *       descriptors).
 *   3.  When no more send descriptors, try to clean-out stale descriptors.
 *
 * Cleaning stale send descriptors:
 *   1.  Listen for virtual interrupt from the server.
 *
 * Receiving packets:
 *   1.  A device thread waits for inbound packet buffers to be allocated.
 *       After the kernel driver thread allocates the packet buffers, and
 *       adds them to a descriptor ring, the kernel thread sends an IPC to
 *       the driver thread.
 *   2.  The device thread waits in an infinite loop for IPC from server.
 *       The IPC string-copies packets into receive buffers.  When low
 *       on socket buffers, the device thread raises virtual interrupt,
 *       and waits for an IPC from the kernel thread.
 *   3.  Device thread adds new packets to a descriptor ring, and
 *       raises a virtual interrupt.
 *   4.  The interrupt handler fetches packets from the inbound descriptor
 *       ring, and injects them into the kernel.  It allocates more buffers
 *       as necessary, and adds them to a buffer descriptor ring.
 */

/****************************************************************************
 *
 * Receive thread functions.
 *
 ****************************************************************************/

/*
 *  1. Use a large descriptor ring.
 *  2. Perform an IPC wait on a group of descriptors.
 *  3. Move to the next group of descriptors for the next IPC.
 *  4. If insufficient descriptors, then send an IPC to the device driver
 *     and wait for replenishment of descriptors.
 *  Note: when the device driver walks the ring of received packets, it must
 *  be able to handle holes ... packets which were not delivered.  It should
 *  just skip the holes and reuse the already allocated buffers.  We tell
 *  the device driver to skip the packet by giving it a buffer length of zero.
 */

static void L4VMnet_rcv_thread_prepare(
	L4VMnet_rcv_group_t *group,
	L4VMnet_client_adapter_t *adapter,
	L4_ThreadId_t *reply_tid,
	L4_MsgTag_t *reply_tag )
{
    L4VMnet_desc_ring_t *ring = &group->rcv_ring;
    L4_Word16_t total, i;
    L4_ThreadId_t from_tid;

    while( 1 )
    {
	// Do we have enough buffers?
	total = 0;
	i = ring->start_free;
	while( 1 )
	{
	    if( !group->rcv_shadow[i].skb ||
		    !ring->desc_ring[i].status.X.server_owned )
	    {
		group->status |= L4VMNET_IRQ_RING_EMPTY;
		break; // Insufficient buffers.
	    }

	    total++;
	    if( total >= IVMnet_rcv_buffer_cnt )
		break;  // We found enough buffers.

	    i = (i + 1) % ring->cnt;
	    if( i == ring->start_free )
	    {
		group->status |= L4VMNET_IRQ_RING_EMPTY;
		break; // Wrapped around the ring.
	    }
	}

	// Calculate whether to send an interrupt.
	if( ((group->status & group->mask) != 0) && !adapter->irq_pending )
	{
	    adapter->irq_pending = TRUE;
	    if( vcpu_in_dispatch_ipc() )
		*reply_tid = L4VM_linux_main_thread( smp_processor_id() );
	    else
		*reply_tid = L4VM_linux_irq_thread( smp_processor_id() );
	    reply_tag->raw = 0;
	    reply_tag->X.label = L4_TAG_IRQ;
	    reply_tag->X.u = 1;
	    L4_LoadMR( 1, L4VMnet_irq );
	}
	else
	    *reply_tid = L4_nilthread;

	// Do the interrupt IPC.
	if( group->status & L4VMNET_IRQ_RING_EMPTY )
	{
	    // We need a wait phase for a reply.
	    L4_MsgTag_t tag;
	    dprintk( 3, PREFIX "available buffers: %d, %d needed; device thread sleeping\n", total, IVMnet_rcv_buffer_cnt );
	    L4_Set_MsgTag( *reply_tag );
	    tag = L4_Ipc( *reply_tid, L4_anythread, 
		    L4_Timeouts(L4_Never,L4_Never), &from_tid );
	    if( L4_IpcFailed(tag) && ((L4_ErrorCode() & 1) == 0) )
	    {
		// Retry to the IRQ dispatcher.
		L4_Set_MsgTag( *reply_tag );
		*reply_tid = L4VM_linux_irq_thread( smp_processor_id() );
		tag = L4_Ipc( *reply_tid, L4_anythread,
			L4_Timeouts(L4_Never,L4_Never), &from_tid );
	    }
	    dprintk( 3, PREFIX "device thread wakeup.\n" );
	}
	else
	{
	    // TODO: study performance impact of a one-way IRQ IPC to notify
	    // the client's IRQ thread of ready packets.  Currently we donate
	    // the timeslice to the IRQ thread, which may be inappropriate.
	    return;
	}
    }
}

static void L4VMnet_rcv_thread_wait(
	L4VMnet_rcv_group_t *group,
	L4VMnet_client_adapter_t *adapter,
	L4_ThreadId_t reply_tid,
	L4_MsgTag_t reply_tag )
{
    L4_Acceptor_t acceptor;
    L4_StringItem_t string_item;
    L4_MsgTag_t msg_tag;
    L4_ThreadId_t server_tid;
    L4_Word16_t i, pkt;
    L4VMnet_desc_ring_t *ring = &group->rcv_ring;
    IVMnet_ring_descriptor_t *desc;

    // Create and install the IPC mapping acceptor.
    acceptor = L4_MapGrantItems( L4_Nilpage );	// No mappings permitted.
    L4_Accept( acceptor );			// Install.
    L4_Set_MsgTag( reply_tag );

    // Install string item acceptors for each packet buffer.
    dprintk( 4, PREFIX "start_free = %d\n", ring->start_free );
    pkt = ring->start_free;
    for( i = 0; i < IVMnet_rcv_buffer_cnt; i++ )
    {
	desc = &ring->desc_ring[ pkt ];
	pkt = (pkt + 1) % ring->cnt;

	ASSERT( desc->status.X.server_owned );

	string_item = L4_StringItem( desc->buffer_len, (void *)desc->buffer );
	if( i < (IVMnet_rcv_buffer_cnt-1) )
	    string_item.X.C = 1; // More buffers comming.

	// Write the message registers.
	L4_LoadBRs( i*2 + 1, 2, string_item.raw );
    }

    // Wait for packets from a valid sender, and send an IRQ IPC if necessary.
    do {
	adapter->client_shared->receiver_tids[ group->group_no ] = L4_Myself();
	msg_tag = L4_Ipc( reply_tid, L4_anythread, 
		L4_Timeouts(L4_Never, L4_Never), &server_tid );
	reply_tid = L4_nilthread;
    } while( msg_tag.X.flags & 8 );

    dprintk( 4, PREFIX "string copy received, string items: %lu\n",
	    L4_TypedWords(msg_tag)/2 );

    // Swallow the received packets.
    for( i = L4_UntypedWords(msg_tag) + 1;
	 (i+1) <= (L4_UntypedWords(msg_tag) + L4_TypedWords(msg_tag));
	 i += 2 )
    {
	// Read the message registers.
	L4_StoreMRs( i, 2, string_item.raw );
	// Release ownership of the socket buffers.
	// TODO: handle invalid IPC message format.
	desc = &ring->desc_ring[ ring->start_free ];
	ASSERT( L4_IsStringItem(&string_item) );
	ASSERT( desc->status.X.server_owned );
	desc->buffer_len = string_item.X.string_length;
	dprintk( 4, PREFIX "string length: %u\n", desc->buffer_len );
	desc->status.X.server_owned = 0;

	ring->start_free = (ring->start_free + 1) % ring->cnt;
	if( desc->buffer_len > 0 )
	    group->status |= L4VMNET_IRQ_PKTS_AVAIL;
    }
}

typedef struct
{
    L4VMnet_client_adapter_t *adapter;
    L4VMnet_rcv_group_t *group;
} L4VMnet_rcv_thread_params_t;

static void
L4VMnet_rcv_thread( void *arg )
{
    L4VMnet_rcv_thread_params_t *params = arg;
    L4_MsgTag_t reply_tag;
    L4_ThreadId_t reply_tid;

    while( 1 )
    {
	L4VMnet_rcv_thread_prepare( params->group, params->adapter,
		&reply_tid, &reply_tag );
	L4VMnet_rcv_thread_wait( params->group, params->adapter,
		reply_tid, reply_tag );
    }
}


/****************************************************************************
 *
 * Server commands.
 *
 ****************************************************************************/

static void
L4VMnet_server_stop( L4VMnet_client_adapter_t *adapter )
{
    CORBA_Environment ipc_env = idl4_default_environment;
    unsigned long irq_flags;

    local_irq_save(irq_flags);
    IVMnet_Control_stop( adapter->server_tid, adapter->ivmnet_handle, &ipc_env );
    local_irq_restore(irq_flags);
}

static void
L4VMnet_server_start( L4VMnet_client_adapter_t *adapter )
{
    CORBA_Environment ipc_env = idl4_default_environment;
    unsigned long irq_flags;

    local_irq_save(irq_flags);
    IVMnet_Control_start( adapter->server_tid, adapter->ivmnet_handle, &ipc_env );
    local_irq_restore(irq_flags);

    // TODO: check for error
    dprintk( 1, PREFIX "started the server connection.\n" );
}

/****************************************************************************
 *
 * Server connection functions.
 *
 ****************************************************************************/

static void
L4VMnet_reset_ring( L4VMnet_desc_ring_t *ring,
	L4VMnet_shadow_t *shadow )
{
    L4_Word16_t i;

    for( i = 0; i < ring->cnt; i++ )
    {
	if( !ring->desc_ring[i].status.X.fragment && shadow[i].skb )
	    dev_kfree_skb_any( shadow[i].skb );
    }

    memset( ring->desc_ring, 0, ring->cnt * sizeof(IVMnet_ring_descriptor_t));
    memset( shadow, 0, ring->cnt * sizeof(L4VMnet_shadow_t) );
}

static int
L4VMnet_open( struct net_device *netdev )
{
    L4VMnet_client_adapter_t *adapter = netdev->priv;
    L4_Fpage_t rcv_window;
    idl4_fpage_t idl4_mapping, idl4_server_mapping;
    CORBA_Environment ipc_env;
    unsigned long irq_flags;
    int err, line = __LINE__;
    int i;
    L4_Word_t log2size, size, size2, window_base;

    if( adapter->connected == 1 )
    {
	L4VMnet_server_start( adapter );
	netif_wake_queue( netdev );
	return 0;
    }

    /* Try to obtain a LAN address if we still need one.
     */
    if( adapter->lanaddress_valid == FALSE )
    {
	err = L4VMnet_allocate_lan_address( netdev->dev_addr );
	if( err )
	{
	    line = __LINE__;
	    goto err_lan_address;
	}
	adapter->lanaddress_valid = TRUE;
    }

    // Locate the network server.
    err = L4VM_server_locate( UUID_IVMnet, &adapter->server_tid );
    if( err != 0 )
    {
	line = __LINE__;
	goto err_locate_server;
    }
    dprintk( 2, PREFIX "server tid %lx\n", adapter->server_tid.raw );

    /* Prepare the send ring.
     */
    adapter->send_ring.cnt = IVMnet_snd_descriptor_ring_cnt;
    adapter->send_ring.start_free = 0;
    adapter->send_ring.start_dirty = 0;
    adapter->send_shadow = kmalloc( adapter->send_ring.cnt * sizeof(L4VMnet_shadow_t), GFP_KERNEL );
    if( adapter->send_shadow == NULL )
    {
	err = -ENOMEM; line = __LINE__;
	goto err_shadow_alloc;
    }

    /* Prepare the receive ring.
     */
    for( i = 0; i < NR_RCV_GROUP; i++ )
    {
	adapter->rcv_group[i].rcv_ring.cnt = IVMnet_rcv_buffer_cnt * 4;
	adapter->rcv_group[i].rcv_ring.start_free = 0;
	adapter->rcv_group[i].rcv_ring.start_dirty = 0;
	adapter->rcv_group[i].rcv_ring.desc_ring = NULL;
	adapter->rcv_group[i].rcv_shadow = NULL;
    }

    for( i = 0; i < NR_RCV_GROUP; i++ )
    {
	adapter->rcv_group[i].rcv_shadow = kmalloc( adapter->rcv_group[i].rcv_ring.cnt 
						    * sizeof(L4VMnet_shadow_t), GFP_KERNEL );
	adapter->rcv_group[i].rcv_ring.desc_ring = kmalloc( adapter->rcv_group[i].rcv_ring.cnt 
							    * sizeof(IVMnet_ring_descriptor_t), GFP_KERNEL );
	if( !adapter->rcv_group[i].rcv_shadow || !adapter->rcv_group[i].rcv_ring.desc_ring )
	{
	    err = -ENOMEM; line = __LINE__;
	    goto err_rcv_alloc;
	}
    }

    // Allocate an oversized shared window region, to adjust for alignment.
    // We receive two fpages in the area.  We will align to the largest fpage.
    size  = L4_Size( L4_Fpage(0,sizeof(IVMnet_client_shared_t)) );
    size2 = L4_Size( L4_Fpage(0,sizeof(IVMnet_server_shared_t)) );
    log2size = L4_SizeLog2( L4_Fpage(0, size+size2) );
    err = L4VM_fpage_vmarea_get( log2size, &adapter->shared_window );
    if( err < 0 )
    {
	line = __LINE__;
	goto err_get_vm_area;
    }
    else
	rcv_window = adapter->shared_window.fpage;
    ASSERT( L4_Address(adapter->shared_window.fpage) >=
	    (L4_Word_t)adapter->shared_window.ioremap_addr );
    dprintk( 2, KERN_INFO PREFIX "receive window at %p, size %lu "
	    "(padding starts at %p)\n",
	    (void *)L4_Address(adapter->shared_window.fpage),
	    L4_Size(adapter->shared_window.fpage),
	    adapter->shared_window.ioremap_addr );

    // Attach to the server, and request the shared mapping area.
    ipc_env = idl4_default_environment;
    local_irq_save(irq_flags);
    idl4_set_rcv_window( &ipc_env, rcv_window );
    IVMnet_Control_attach( adapter->server_tid, "eth1", netdev->dev_addr, &adapter->ivmnet_handle, 
			   &idl4_mapping, &idl4_server_mapping, &ipc_env );
    local_irq_restore(irq_flags);
    if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free(&ipc_env);
	err = -EIO; line = __LINE__;
	goto err_server_register;
    }

    dprintk( 1, PREFIX "shared region at base 0x%x, offset 0x%lx, "
	    "size %ld\n",
	    idl4_fpage_get_base(idl4_mapping),
	    L4_Address(idl4_fpage_get_page(idl4_mapping)),
	    L4_Size(idl4_fpage_get_page(idl4_mapping)) );
    dprintk( 1, PREFIX "server status region at base 0x%x, offset 0x%lx, "
	    "size %ld\n",
	    idl4_fpage_get_base(idl4_server_mapping),
	    L4_Address(idl4_fpage_get_page(idl4_server_mapping)),
	    L4_Size(idl4_fpage_get_page(idl4_server_mapping)) );

    // Finish up initialization.
    window_base = L4_Address( rcv_window );
    adapter->client_shared = (IVMnet_client_shared_t *)
	(window_base + idl4_fpage_get_base(idl4_mapping));
    adapter->client_shared->client_irq_tid = L4_nilthread;
    adapter->client_shared->client_irq_pending = 0;
    adapter->send_ring.desc_ring = adapter->client_shared->snd_desc_ring;
    adapter->client_shared->receiver_cnt = 0;

    adapter->server_status = (IVMnet_server_shared_t *)
	(window_base + idl4_fpage_get_base(idl4_server_mapping));

    // Zero the send rings.
    memset( adapter->send_ring.desc_ring, 0, adapter->send_ring.cnt * sizeof(IVMnet_ring_descriptor_t) );
    memset( adapter->send_shadow, 0, adapter->send_ring.cnt * sizeof(L4VMnet_shadow_t) );

    // Zero the receive rings.
    for( i = 0; i < NR_RCV_GROUP; i++ )
    {
	memset( adapter->rcv_group[i].rcv_ring.desc_ring, 0, 
		adapter->rcv_group[i].rcv_ring.cnt * sizeof(IVMnet_ring_descriptor_t) );
	memset( adapter->rcv_group[i].rcv_shadow, 0, 
		adapter->rcv_group[i].rcv_ring.cnt * sizeof(L4VMnet_shadow_t) );
    }

    // More IRQ related initialization.
    adapter->irq_pending = FALSE;

    // Init the receiver threads.
    for( i = 0; i < NR_RCV_GROUP; i++ )
    {
	adapter->rcv_group[i].group_no = i;
	adapter->rcv_group[i].dev_tid = L4_nilthread;
	adapter->rcv_group[i].status = 0;
	adapter->rcv_group[i].mask = 0;
    }

    // Allocate a virtual interrupt.
    if( L4VMnet_irq > NR_IRQS )
    {
	printk( KERN_ERR PREFIX "unable to reserve a virtual interrupt.\n" );
	goto err_vmpic_reserve;
    }
    
    if (L4VMnet_irq == 0)
    {
#if defined(CONFIG_X86_IO_APIC)
        L4_KernelInterfacePage_t *kip = (L4_KernelInterfacePage_t *) L4_GetKernelInterface();
        L4VMnet_irq = L4_ThreadIdSystemBase(kip) + 3;	
	acpi_register_gsi(L4VMnet_irq, ACPI_LEVEL_SENSITIVE, ACPI_ACTIVE_LOW);

#else
	L4VMnet_irq = 9;
#endif
    }
    printk( KERN_INFO PREFIX "L4VMnet client irq %d\n", L4VMnet_irq );

    l4ka_wedge_add_virtual_irq( L4VMnet_irq );
    err = request_irq( L4VMnet_irq, L4VMnet_irq_handler, 0, 
	    netdev->name, netdev );
    if( err < 0 )
    {
	printk( KERN_ERR PREFIX "unable to allocate an interrupt.\n" );
	goto err_request_irq;
    }

    // Tell the server our irq tid.
    adapter->client_shared->client_irq_tid = get_vcpu()->irq_gtid;

    // Start the receiver threads.
    for( i = 0; i < NR_RCV_GROUP; i++ )
    {
	// Prepare parameters for the new thread.
	L4VMnet_rcv_thread_params_t params;
	params.adapter = adapter;
	params.group = &adapter->rcv_group[i];
	// Start the thread.
	adapter->rcv_group[i].dev_tid = L4VM_thread_create( GFP_KERNEL,
		L4VMnet_rcv_thread, l4ka_wedge_get_irq_prio(), 
		0, &params, sizeof(params) );
	if( L4_IsNilThread(adapter->rcv_group[i].dev_tid) )
	{
	    printk( KERN_ERR PREFIX "failed to start a receiver thread\n" );
	    continue;
	}

	// Tell the server about our receiver threads.
	adapter->client_shared->receiver_tids[ adapter->client_shared->receiver_cnt ] = adapter->rcv_group[i].dev_tid;
	adapter->client_shared->receiver_cnt++;
    }

    // Tell the server that we are ready to handle packets.
    adapter->connected = 1;
    L4VMnet_server_start( adapter );
    netif_wake_queue( netdev );

    // Enable the interrupts.
    for( i = 0; i < NR_RCV_GROUP; i++ )
	adapter->rcv_group[i].mask = ~0;

    return 0;

err_request_irq:
err_vmpic_reserve:
err_server_register:
    L4VM_fpage_vmarea_release( &adapter->shared_window );
err_get_vm_area:
    kfree( adapter->send_shadow );
err_rcv_alloc:
    for( i = 0; i < NR_RCV_GROUP; i++ )
    {
	if( adapter->rcv_group[i].rcv_shadow )
	    kfree( adapter->rcv_group[i].rcv_shadow );
	if( adapter->rcv_group[i].rcv_ring.desc_ring )
	    kfree( adapter->rcv_group[i].rcv_ring.desc_ring );
    }
err_shadow_alloc:
err_locate_server:
err_lan_address:
    printk( KERN_ERR PREFIX "configuration error (%s:%d)\n", __FILE__, line );
    return err;
}

static int
L4VMnet_close( struct net_device *netdev )
{
    L4VMnet_client_adapter_t *adapter = netdev->priv;
    CORBA_Environment ipc_env;
    unsigned long irq_flags;
    int i;

    // Stop the server, so that we can clean packets.
    netif_stop_queue( netdev );
    L4VMnet_server_stop( adapter );

    // Preserve our server connection.
    return 0;

    // TODO: coordinate with the device threads!  They are waiting
    // on these buffers!!

    // Drop all packets in the send descriptor ring.
    L4VMnet_reset_ring( &adapter->send_ring, adapter->send_shadow );

    // Drop all packets in the receive descriptor rings.
    for( i = 0; i < NR_RCV_GROUP; i++ )
	L4VMnet_reset_ring( &adapter->rcv_group[i].rcv_ring,
		adapter->rcv_group[i].rcv_shadow );

    // Disconnect from the server.
    ipc_env = idl4_default_environment;
    local_irq_save(irq_flags);
    IVMnet_Control_detach( adapter->server_tid, adapter->ivmnet_handle, &ipc_env );
    local_irq_restore(irq_flags);
    if( ipc_env._major == CORBA_USER_EXCEPTION )
    {
	CORBA_exception_free(&ipc_env);
	printk( KERN_ERR PREFIX "server doesn't acknowledge our connection\n" );
    }
    else if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free(&ipc_env);
	printk( KERN_ERR PREFIX "unable to disconnect from server\n" );
	return -EIO;
    }

    // Free receiver stuff.
    for( i = 0; i < NR_RCV_GROUP; i++ )
    {
	L4VM_thread_delete( adapter->rcv_group[i].dev_tid );
	kfree( adapter->rcv_group[i].rcv_shadow );
	kfree( adapter->rcv_group[i].rcv_ring.desc_ring );
    }

    // Unregister the IRQ handler.
    // TODO: release the IRQ handler.

    // Free resources.
    L4VM_fpage_vmarea_release( &adapter->shared_window );
    kfree( adapter->send_shadow );

    adapter->connected = 0;
    return 0;
}

static struct net_device_stats *
L4VMnet_get_stats( struct net_device *netdev )
{
    L4VMnet_client_adapter_t *adapter = netdev->priv;

    if( NULL != adapter && FALSE != adapter->connected ) {
	CORBA_Environment ipc_env;
	unsigned long irq_flags;

	ipc_env = idl4_default_environment;
	local_irq_save(irq_flags);
	IVMnet_Control_update_stats( adapter->server_tid, adapter->ivmnet_handle, &ipc_env );
	local_irq_restore(irq_flags);

	if( ipc_env._major == CORBA_NO_EXCEPTION ) {
	    return (struct net_device_stats *)&(adapter->server_status->stats);
	} else if( ipc_env._major == CORBA_USER_EXCEPTION ) {
	    printk( KERN_ERR PREFIX "exception %d while requesting stats update from server\n",
		    ipc_env._minor );
	} else {
	    printk( KERN_ERR PREFIX "unable to request stats update from server\n" );
	}

	L4_KDB_Enter("get_stats");
	CORBA_exception_free(&ipc_env);
    }

    return NULL;
}

/****************************************************************************
 *
 * Packet send functions.
 *
 ****************************************************************************/

static void
L4VMnet_clean_send_ring( L4VMnet_client_adapter_t *adapter )
{
    IVMnet_ring_descriptor_t *desc;
    L4VMnet_desc_ring_t *ring = &adapter->send_ring;
    L4_Word16_t cleaned = 0;

    while( 1 )
    {
	desc = &ring->desc_ring[ ring->start_dirty ];
	if( desc->status.X.server_owned )
	    break;

	if( adapter->send_shadow[ ring->start_dirty ].skb == NULL )
	{
	    ASSERT( ring->start_dirty == ring->start_free );
	    break;
	}

	if( !desc->status.X.fragment )
	    dev_kfree_skb_any( adapter->send_shadow[ ring->start_dirty ].skb );
	adapter->send_shadow[ ring->start_dirty ].skb = NULL;
	desc->buffer = 0;
	desc->buffer_len = 0;

	ring->start_dirty = (ring->start_dirty + 1) % ring->cnt;
	cleaned++;
    }

    // Wake the queue *after* we finish playing with the ring descriptors,
    // so that we avoid concurrent access to this function!
    if( cleaned && netif_queue_stopped(adapter->dev) )
    {
	netif_wake_queue( adapter->dev );
	dprintk( 3, PREFIX "rewaking packet queue\n" );
    }
    else if( netif_queue_stopped(adapter->dev) )
    {
//	dprintk( 3, PREFIX "send packet queue jammed\n" );
    }
}

static void
L4VMnet_trigger_send( L4VMnet_client_adapter_t *adapter )
{
    L4_MsgTag_t msg_tag, result_tag;

    adapter->server_status->irq_status |= 1;
    if( adapter->server_status->irq_pending )
	return;

    adapter->server_status->irq_pending = TRUE;

    {

	unsigned long flags;
	local_irq_save(flags);
	{
	    L4_ThreadId_t from_tid;
	    msg_tag.raw = 0; msg_tag.X.label = L4_TAG_IRQ; msg_tag.X.u = 1;
	    L4_Set_MsgTag( msg_tag );
	    L4_LoadMR( 1, adapter->client_shared->server_irq );
	    result_tag = L4_Reply( adapter->client_shared->server_main_tid );
	    if( !L4_IpcSucceeded(result_tag) )
	    {
		L4_Set_MsgTag( msg_tag );
		L4_Ipc( adapter->client_shared->server_irq_tid, L4_nilthread,
	    		L4_Timeouts(L4_Never, L4_Never), &from_tid);
	    }
	}
	local_irq_restore(flags);
    }
}

static int
L4VMnet_xmit_available(
	L4VMnet_client_adapter_t *adapter,
	L4_Word16_t fragments )
{
    L4VMnet_desc_ring_t *ring = &adapter->send_ring;
    IVMnet_ring_descriptor_t *desc;
    int dirty = FALSE;
    int frag, i;

    ASSERT( fragments < ring->cnt );

    if( L4VMnet_ring_available(ring) <= fragments )
    {
	i = ring->start_dirty;
	for( frag = 0; frag < fragments; frag++ )
	{
	    desc = &ring->desc_ring[ i ];
	    if( desc->status.X.server_owned )
		return FALSE;
	    dirty = TRUE;
	    i = (i + 1) % ring->cnt;
	}
    }

    if( dirty )
    {
	L4VMnet_clean_send_ring( adapter );
	ASSERT( adapter->send_shadow[ ring->start_free ].skb == NULL );
    }

    return TRUE;
}

static int
L4VMnet_xmit_frame( struct sk_buff *skb, struct net_device *netdev )
{
    L4VMnet_client_adapter_t *adapter = netdev->priv;
    L4VMnet_desc_ring_t *ring = &adapter->send_ring;
    IVMnet_ring_descriptor_t *desc = &ring->desc_ring[ ring->start_free ];
    IVMnet_ring_descriptor_t *frag_desc;
    L4_Word16_t fragments, f;
    unsigned long flags;

    fragments = 1 + skb_shinfo(skb)->nr_frags;
    if( unlikely(fragments >= ring->cnt) )
    {
	// Too many fragments in the packet, so drop the packet.
	dev_kfree_skb_any( skb );
	return 0;
    }

    local_irq_save(flags);
    if( !L4VMnet_xmit_available(adapter, fragments) )
    {
	// We are still waiting for the server to empty the send queue.
	netif_stop_queue( netdev );
	local_irq_restore(flags);
	tasklet_hi_schedule( &L4VMnet_flush_tasklet );
	dprintk( 3, PREFIX "send queue full\n" );
	return 1;
    }
    local_irq_restore(flags);

    dprintk( 4, PREFIX "skb protocol %d, pkt_type %d\n",
	    skb->protocol, skb->pkt_type );

    ASSERT( 0 == desc->status.X.server_owned );
    ASSERT( NULL == adapter->send_shadow[ ring->start_free ].skb );
    desc->status.raw	= 0;
    desc->buffer	= virt_to_phys( skb->data );
    desc->buffer_len	= skb_headlen( skb );
    desc->pkt_type	= skb->pkt_type;
    desc->protocol	= skb->protocol;
    adapter->send_shadow[ ring->start_free ].skb = skb;
    // Where the device must start the checksum, relative to the buffer.
    desc->csum.tx.start = skb->h.raw - skb->data;
    // Where the device must store the checksum, relative to the buffer.
    desc->csum.tx.offset = desc->csum.tx.start + skb->csum;
    if( skb->ip_summed == CHECKSUM_HW )
	desc->status.X.csum = 1;
    ring->start_free = (ring->start_free + 1) % ring->cnt;

    dprintk( 4, PREFIX "send pkt @ virt %lx, bus %lx, size %d\n",
	     (unsigned long) skb->data, (unsigned long)  desc->buffer, (unsigned long)  desc->buffer_len );

    // Queue packet fragments, for scatter/gather.
    frag_desc = desc;
    for( f = 0; f < skb_shinfo(skb)->nr_frags; f++ )
    {
	skb_frag_t *frag = &skb_shinfo(skb)->frags[f];

	dprintk( 4, PREFIX "frag %d @ %lx + %lx, %d --> %lx\n",
		f,
		(L4_Word_t)frag->page, (L4_Word_t)frag->page_offset,
		frag->size, L4VMnet_fragment_buffer(frag));

	// Mark the *prior* descriptor as a fragment.  The last descriptor
	// in the sequence is not marked.
	frag_desc->status.X.fragment = 1;

	frag_desc = &ring->desc_ring[ ring->start_free ];
	ASSERT( 0 == frag_desc->status.X.server_owned );
	ASSERT( NULL == adapter->send_shadow[ ring->start_free ].skb );

	frag_desc->status.raw	= 0;
	frag_desc->buffer	= L4VMnet_fragment_buffer(frag);
	frag_desc->buffer_len	= frag->size;
	adapter->send_shadow[ ring->start_free ].skb = skb;
	frag_desc->status.X.server_owned = 1;
	ring->start_free = (ring->start_free + 1) % ring->cnt;
    }

    // Assign ownership of the packet sequence to the server.  Do this last!
    desc->status.X.server_owned = 1;

    // Deal with sending the packets.
    netdev->trans_start = jiffies;
    dprintk( 4, PREFIX "send %u fragments\n", fragments );

    tasklet_hi_schedule( &L4VMnet_flush_tasklet );

    return 0;
}

static void
L4VMnet_flush_tasklet_handler( unsigned long unused )
{
    L4VMnet_client_adapter_t *adapter = L4VMnet_adapter_list;

    L4VMnet_trigger_send( adapter );
}

/****************************************************************************
 *
 * Packet receive functions.
 *
 ****************************************************************************/

static void L4VMnet_wake_driver_thread(
	L4VMnet_rcv_group_t *rcv_group,
	L4VMnet_client_adapter_t *adapter )
{
    L4_MsgTag_t tag;
    unsigned long flags;

    local_irq_save(flags);
    tag.raw = 0;
    L4_Set_MsgTag( tag );
    L4_Reply( rcv_group->dev_tid );
    local_irq_restore(flags);
}

static void L4VMnet_walk_rcv_ring(
	int wake,
	L4VMnet_rcv_group_t *group,
	L4VMnet_client_adapter_t *adapter )
{
    volatile IVMnet_ring_descriptor_t *desc;
    L4VMnet_desc_ring_t *ring = &group->rcv_ring;
    struct sk_buff *skb;
    L4_Word16_t pkt_len = IVMnet_rcv_buffer_size;
    L4_Word16_t reserve_len = 2;
    int total_transferred = 0;

    // Descriptors can be in several states:
    //  1. Zero buffer len: either the buffer doesn't exist, or it was a
    //     dropped packet and the buffer exists.
    //  2. Valid packet that should be handed off to Linux.
    //  3. Packet still belongs to the driver thread.

    dprintk( 4, PREFIX "start_dirty = %d\n", ring->start_dirty );
    while( 1 )
    {
	desc = &ring->desc_ring[ ring->start_dirty ];
	skb = group->rcv_shadow[ ring->start_dirty ].skb;

	if( desc->status.X.server_owned )
	    break;

	if( likely((desc->buffer_len > 0) && skb) )
	{
	    // Give the skb to the Linux network stack.
	    dprintk( 4, PREFIX "injesting packet\n" );
	    skb_put( skb, desc->buffer_len );
	    // Assume that the DD/OS performed the checksum.
	    skb->ip_summed = CHECKSUM_UNNECESSARY;
	    skb->protocol = eth_type_trans( skb, adapter->dev );
	    netif_rx( skb );
	    adapter->dev->last_rx = jiffies;
	    skb = NULL;  // Release the skb.
	}

	if( skb == NULL )
	{
	    // Allocate a new skb.
	    skb = dev_alloc_skb( pkt_len + reserve_len );
	    group->rcv_shadow[ ring->start_dirty ].skb = skb;

	    if( unlikely(skb == NULL) )
	    {
		desc->buffer = 0;
		desc->buffer_len = 0;
	    }
	    else
	    {
		// Align the buffer 2 beyond a 16-byte boundary, to result
		// in a 16-byte aligned IP header after the 14-byte MAC header
		// is removed.
		skb_reserve( skb, reserve_len );
		skb->dev = adapter->dev;
	    }
	}

	if( likely(skb != NULL) )
	{
	    // This could be a reused buffer, or a new buffer.
	    desc->buffer = (L4_Word_t)skb->data;
	    desc->buffer_len = pkt_len;
	    // Transfer ownership to the device.
	    desc->status.raw = 0;
	    desc->status.X.server_owned = 1;
	    total_transferred++;

	    ring->start_dirty = (ring->start_dirty + 1) % ring->cnt;
	}
	else
	    break; // Insufficient memory!
    }

    // TODO: deal with insufficient memory conditions.

    dprintk( 3, PREFIX "allocated %d receive buffers.\n", total_transferred );
    if( total_transferred && wake )
	L4VMnet_wake_driver_thread( group, adapter );
}

static irqreturn_t
L4VMnet_irq_handler( int irq, void *data, struct pt_regs *regs )
{
    struct net_device *netdev = data;
    L4VMnet_client_adapter_t *adapter = netdev->priv;
    L4VMnet_rcv_group_t *rcv_group;
    int i, finished, wake, cnt = 0;

    do
    {
	// No one needs to deliver IRQ messages.  They would be
	// superfluous.
	adapter->irq_pending = TRUE;
	finished = TRUE;

	// Process receive interrupts.
	for( i = 0; i < NR_RCV_GROUP; i++ )
	{
	    unsigned events;
	    rcv_group = &adapter->rcv_group[i];

	    do {
		events = rcv_group->status;
	    } while( cmpxchg(&rcv_group->status, events, 0) != events );

	    if( !events )
		continue;

	    finished = FALSE;

	    wake = 0 != (events & L4VMNET_IRQ_RING_EMPTY);
	    dprintk( 3, PREFIX "irq handler, group %d, wake %d: 0x%x\n",
		    i, wake, events );
	    L4VMnet_walk_rcv_ring( wake, rcv_group, adapter );
	}

	// Process transmit related problems.
	if( netif_queue_stopped(adapter->dev) )
	    L4VMnet_clean_send_ring( adapter );

	// Enable interrupt message delivery.
	adapter->irq_pending = FALSE;
	cnt++;
	if( cnt > 100 )
	    dprintk( 1, PREFIX "too many interrupts.\n" );
    } while( !finished );

    return IRQ_HANDLED;
}

static int
L4VMnet_irq_pending( void *data )
{
    L4VMnet_client_adapter_t *adapter = (L4VMnet_client_adapter_t *)data;
    int i;

    for( i = 0; i < NR_RCV_GROUP; i++ )
	if( (adapter->rcv_group[i].status & adapter->rcv_group[i].mask) != 0 )
	{
	    dprintk( 4, PREFIX "irq pending.\n" );
	    return TRUE;
	}
    return FALSE;
}

/****************************************************************************
 *
 * Module-level functions.
 *
 ****************************************************************************/

static int __init
L4VMnet_client_init_module( void )
{
    struct net_device *netdev;
    L4VMnet_client_adapter_t *adapter;
    int err;

    netdev = alloc_etherdev( sizeof(L4VMnet_client_adapter_t) );
    if( !netdev )
	return -ENOMEM;

    SET_MODULE_OWNER( netdev );

    /* Adapter should already be zeroed. */
    adapter = (L4VMnet_client_adapter_t *)netdev->priv;
    adapter->dev = netdev;
    adapter->connected = 0;
    L4VMnet_adapter_list = adapter;

    /* Initialize the netdev. */
    netdev->open = L4VMnet_open;
    netdev->stop = L4VMnet_close;
    netdev->hard_start_xmit = L4VMnet_xmit_frame;
    netdev->features = NETIF_F_SG | NETIF_F_HW_CSUM | NETIF_F_FRAGLIST;
    netdev->get_stats = L4VMnet_get_stats;

    /* Try to obtain a LAN address.  We can retry later if necessary. */
    memset( netdev->dev_addr, 0, ETH_ALEN );
    adapter->lanaddress_valid =
	( 0 == L4VMnet_allocate_lan_address(netdev->dev_addr) );
    printk( KERN_INFO PREFIX "LAN address: " );
    L4VMnet_print_lan_address( netdev->dev_addr );
    printk( "\n" );

    /* Register the netdev. */
    err = register_netdev( netdev );
    if( err )
	goto err_register_netdev;
    netif_stop_queue( netdev );

    printk( KERN_INFO PREFIX "L4VMnet client device %s\n", netdev->name );

    return 0;

err_register_netdev:
    kfree( netdev );
    return err;
}

static void __exit
L4VMnet_client_exit_module( void )
{
    if( L4VMnet_adapter_list )
    {
	unregister_netdev( L4VMnet_adapter_list->dev );
	kfree( L4VMnet_adapter_list->dev );
	L4VMnet_adapter_list = NULL;
    }
}

MODULE_AUTHOR( "Joshua LeVasseur <jtl@ira.uka.de>" );
MODULE_DESCRIPTION( "L4 network client driver" );
MODULE_LICENSE( "Dual BSD/GPL" );
MODULE_SUPPORTED_DEVICE( "L4 VM net" );
MODULE_VERSION("slink");

module_init( L4VMnet_client_init_module );
module_exit( L4VMnet_client_exit_module );

#if defined(MODULE)
// Define a .afterburn_wedge_module section, to activate symbol resolution
// for this module against the wedge's symbols.
__attribute__ ((section(".afterburn_wedge_module")))
burn_wedge_header_t burn_wedge_header = {
    BURN_WEDGE_MODULE_INTERFACE_VERSION
};
#endif

