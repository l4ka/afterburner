/*********************************************************************
 *                
 * Copyright (C) 2004 Joshua LeVasseur
 *
 * File path:	linuxblock/L4VMblock_server.c
 * Description:	
 *
 * Proprietary!  DO NOT DISTRIBUTE!
 *
 * $Id: server.c,v 1.1 2006/09/21 09:28:35 joshua Exp $
 *                
 ********************************************************************/

#define DO_L4VMBLOCK_REORDER

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>

#include <asm/io.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#include <linux/blk.h>
#include <asm/irq.h>
#include <asm/arch/hardirq.h>
#include <asm/l4lxapi/thread.h>
#include <asm/l4linux/debug.h>
#else
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/kdev_t.h>
typedef dev_t kdev_t;
#define herc_printf L4_printk
#endif

#define PREFIX "L4VMblock server: "

#include "server.h"

#if !defined(CONFIG_AFTERBURN_DRIVERS_BLOCK_OPTIMIZE)
int L4VMblock_debug_level = 2;
MODULE_PARM( L4VMblock_debug_level, "i" );
#endif

static L4_Word_t L4VMblock_irq_no;
static L4VMblock_server_t L4VMblock_server;

static void L4VMblock_notify_tasklet_handler( unsigned long unused );
DECLARE_TASKLET( L4VMblock_notify_tasklet, L4VMblock_notify_tasklet_handler, 0);

/***************************************************************************
 *
 * Linux block functions.
 *
 ***************************************************************************/

static void
L4VMblock_deliver_client_irq( L4VMblock_client_info_t *client )
{
    CORBA_Environment ipc_env = idl4_default_environment;

    client->client_shared->client_irq_status |= L4VMBLOCK_CLIENT_IRQ_CLEAN;
#if 0
    if( client->client_shared->client_irq_pending )
	return;
#endif

    client->client_shared->client_irq_pending = TRUE;

#if 0
    // Send a zero-timeout IPC.
    ipc_env._timeout = L4_Timeouts( L4_ZeroTime, L4_Never );
#endif

    ILinux_raise_irq( client->client_shared->client_irq_tid,
	    client->client_shared->client_irq_no, &ipc_env );

    if( ipc_env._major != CORBA_NO_EXCEPTION )
	CORBA_exception_free( &ipc_env );
}

static void
L4VMblock_notify_tasklet_handler( unsigned long unused )
{
    IVMblock_handle_t handle;
    L4VMblock_client_info_t *client;
    L4VMblock_server_t *server = &L4VMblock_server;

    for( handle = 0; handle < L4VMBLOCK_MAX_CLIENTS; handle++ )
    {
	client = L4VMblock_client_lookup( server, handle );
	if( client )
	    L4VMblock_deliver_client_irq( client );
    }
}

static void
L4VMblock_end_io( struct buffer_head *bh, int uptodate )
{
    L4VMblock_server_t *server = &L4VMblock_server;
    IVMblock_ring_descriptor_t *desc = (IVMblock_ring_descriptor_t *)
	bh->b_private;
    L4VMblock_conn_info_t *conn;

    dprintk( 4, KERN_INFO PREFIX "io completed, up to date? %d\n", uptodate );

    conn = L4VMblock_conn_lookup( server, desc->handle );
    if( !conn )
	dprintk( 1, KERN_INFO PREFIX "io completed, but connection is gone.\n");
    else
    {
#if defined(DO_L4VMBLOCK_REORDER)
	L4VMblock_client_info_t *client = conn->client;
	L4VMblock_ring_t *ring_info = &client->ring_info;
	L4_Word16_t later_idx, earlier_idx;

	// Find location of the descriptor in the ring.
	later_idx = ((L4_Word_t)desc - (L4_Word_t)client->client_shared->desc_ring) / sizeof(IVMblock_ring_descriptor_t);
	earlier_idx = ring_info->start_dirty;

	ASSERT( client->client_shared->desc_ring[earlier_idx].status.X.server_owned );

	// Is it out-of-order?
	if( later_idx != earlier_idx )
	{
	    IVMblock_ring_descriptor_t tmp_desc;
	    struct buffer_head *tmp_bh;

	    // Make temporary copies of the later info.
	    tmp_desc = client->client_shared->desc_ring[later_idx];
	    tmp_bh = client->bh_ring[later_idx];

	    ASSERT( client->client_shared->desc_ring[earlier_idx].status.X.server_owned );
	    ASSERT( desc == &client->client_shared->desc_ring[later_idx] );
	    ASSERT( bh == tmp_bh );

	    // Move the earlier info to the later slots.
	    client->bh_ring[later_idx] = client->bh_ring[earlier_idx];
	    client->client_shared->desc_ring[later_idx] = client->client_shared->desc_ring[earlier_idx];
	    // Update the bh's private data.
	    client->bh_ring[later_idx]->b_private = (void *)
		&client->client_shared->desc_ring[later_idx];

	    // Move the temporary copies into the earlier slots.
	    client->bh_ring[earlier_idx] = tmp_bh;
	    client->client_shared->desc_ring[earlier_idx] = tmp_desc;

	    // Update our desc pointer.
	    desc = &client->client_shared->desc_ring[earlier_idx];

	    dprintk( 4, KERN_INFO PREFIX "out-of-order, current: %d, "
		    "done: %d\n",
		    earlier_idx, later_idx );
	}
	ring_info->start_dirty = (ring_info->start_dirty + 1) % ring_info->cnt;
#endif

	if( !uptodate )
    	    desc->status.X.server_err = 1;
	desc->status.X.server_owned = 0;

//	L4VMblock_deliver_client_irq( conn->client );
	tasklet_schedule( &L4VMblock_notify_tasklet );
    }
}

static void L4VMblock_initiate_io( 
	L4VMblock_conn_info_t *conn,
	IVMblock_ring_descriptor_t *desc,
	struct buffer_head *bh )
{
    // Modeled from brw_kiovec() and generic_direct_IO() in linux/fs/buffer.c
    int rw;
    if( desc->status.X.do_write )
	rw = WRITE;
    else if( desc->status.X.speculative )
	rw = READA;
    else
	rw = READ;

    if( desc->size > conn->block_size )
    {
	printk( KERN_ERR PREFIX "client requested block size %lu, but the "
		"device wants %lu\n", desc->size, conn->block_size );
	desc->status.X.server_err = 1;
	desc->status.X.server_owned = 0;
	return;
    }

    /* From init_buffer_head() in fs/dcache.c. */
    memset( bh, 0, sizeof(*bh) );
    init_waitqueue_head( &bh->b_wait );

    init_buffer( bh, L4VMblock_end_io, /* private */ desc );
    bh->b_dev = conn->kdev;
    bh->b_rdev = conn->kdev;
    bh->b_size = desc->size;
    bh->b_rsector = desc->offset;
    bh->b_blocknr = bh->b_rsector / (bh->b_size >> 9);
    bh->b_this_page = bh;
    bh->b_state = (1 << BH_Uptodate) | (1 << BH_Lock) | (1 << BH_Mapped);
    if( desc->status.X.do_write )
	clear_bit( BH_Dirty, &bh->b_state );

    bh->b_page = NULL;
    bh->b_data = bus_to_virt( (L4_Word_t)desc->page );
    if( unlikely(bh->b_data == NULL) )
    {
	// TODO: reorder!!
	desc->status.X.server_err = 1;
	desc->status.X.server_owned = 0;
	return;
    }
    atomic_inc( &bh->b_count );
 
    dprintk( 4, KERN_INFO PREFIX "i/o request, size %lu, page %p, offset %lu, "
	    "write? %d, bus addr %p, vaddr %p (%p)\n",
	    desc->size, desc->page, desc->offset, desc->status.X.do_write,
	    (void *)virt_to_bus(bh->b_data), 
	    bh->b_data, bus_to_virt(virt_to_bus(bh->b_data)) );

    submit_bh( rw, bh );

}

static void L4VMblock_process_io_queue(
	L4VMblock_server_t *server,
	L4VMblock_client_info_t *client )
{
    L4VMblock_ring_t *ring_info = &client->ring_info;
    IVMblock_ring_descriptor_t *desc;
    struct buffer_head *bh;
    L4VMblock_conn_info_t *conn;
    L4_Word_t start_free = ring_info->start_free;

    while( 1 )
    {
	// Get stuff from the appropriate rings.
	desc = &client->client_shared->desc_ring[ ring_info->start_free ];
	if( !desc->status.X.server_owned )
	    break;
	bh = client->bh_ring[ ring_info->start_free ];

	// Start the I/O (if possible).
	conn = L4VMblock_conn_lookup( server, desc->handle );
	if( conn && (conn->client == client) )
	    L4VMblock_initiate_io( conn, desc, bh );
	else
	{
	    // TODO: reorder!!
	    printk( KERN_ERR PREFIX "client request accesses invalid connection.\n" );
	    desc->status.X.server_err = 1;
	    desc->status.X.server_owned = 0;
	}

	// Move to the next descriptor.
	ring_info->start_free = (ring_info->start_free + 1) % ring_info->cnt;
	if( ring_info->start_free == start_free )
	{
	    dprintk( 2, KERN_INFO PREFIX "client caused a wrap-around.\n" );
	    break;	// Wrapped around.
	}
    }
}

static void
L4VMblock_process_client_io( L4VMblock_server_t *server )
{
    int i;
    L4VMblock_client_info_t *client;

    for( i = 0; i < L4VMBLOCK_MAX_CLIENTS; i++ )
    {
	client = L4VMblock_client_lookup( server, i );
	if( client )
	    L4VMblock_process_io_queue( server, client );
    }

    // Start execution of our queued block requests.
    run_task_queue( &tq_disk );
}

/***************************************************************************
 *
 * Server handlers.
 *
 ***************************************************************************/

static void
L4VMblock_probe_handler( L4VM_server_cmd_t *cmd, void *data )
{ 
    L4VMblock_server_t *server = (L4VMblock_server_t *)data;
    idl4_server_environment ipc_env;
    IVMblock_devprobe_t probe_data;
    L4VM_server_cmd_params_t *params = (L4VM_server_cmd_params_t *)cmd->data;
    kdev_t kdev;

    ASSERT( cmd->reply_to_tid.raw );
    ASSERT( params );
    ASSERT( server->server_tid.raw );
    ipc_env._action = 0;
    kdev = MKDEV( params->probe.devid.major, params->probe.devid.minor );

    dprintk( 2, KERN_INFO PREFIX "probe request from %p for device %u:%u\n", RAW(cmd->reply_to_tid), MAJOR(kdev), MINOR(kdev) );

    if( !bdget(kdev_t_to_nr(kdev)) )
    {
	CORBA_exception_set( &ipc_env, ex_IVMblock_invalid_device, NULL );
    }
    else
    {
	// TODO: acquire some lock, for in case the device disappears
	// during our lookups of minor devices.
	probe_data.devid = params->probe.devid;
	probe_data.device_size = blk_size[MAJOR(kdev)] ? blk_size[MAJOR(kdev)][MINOR(kdev)] : 0;
	probe_data.block_size = block_size(kdev);
	probe_data.hardsect_size = get_hardsect_size(kdev);
	probe_data.read_ahead = read_ahead[MAJOR(kdev)];
	probe_data.max_read_ahead = max_readahead[MAJOR(kdev)] ? max_readahead[MAJOR(kdev)][MINOR(kdev)] : 31;
	probe_data.req_max_sectors = max_sectors[MAJOR(kdev)] ? max_sectors[MAJOR(kdev)][MINOR(kdev)] : MAX_SECTORS;
    }

    L4_Set_VirtualSender( server->server_tid );
    IVMblock_Control_probe_propagate_reply( cmd->reply_to_tid,
	    &probe_data, &ipc_env );
}

static int L4VMblock_allocate_conn_handle( 
	L4VMblock_server_t *server,
	IVMblock_handle_t *handle )
{
    static spinlock_t lock = SPIN_LOCK_UNLOCKED;

    spin_lock( lock );
    {
	int i;
	for( i = 0; i < L4VMBLOCK_MAX_DEVICES; i++ )
	    if( server->connections[i].handle == L4VMBLOCK_INVALID_HANDLE )
	    {
		*handle = i;
		server->connections[i].handle = *handle;
		spin_unlock( lock );
		return TRUE;
	    }
    }
    spin_unlock( lock );

    return FALSE;
}

static int L4VMblock_allocate_client_handle(
	L4VMblock_server_t *server,
	IVMblock_handle_t *handle )
{
    static spinlock_t lock = SPIN_LOCK_UNLOCKED;

    spin_lock( lock );
    {
	int i;
	for( i = 0; i < L4VMBLOCK_MAX_CLIENTS; i++ )
	    if( server->clients[i].handle == L4VMBLOCK_INVALID_HANDLE )
	    {
		*handle = i;
		server->clients[i].handle = *handle;
		spin_unlock( lock );
		return TRUE;
	    }
    }
    spin_unlock( lock );

    return FALSE;
}

static void
L4VMblock_attach_handler( L4VM_server_cmd_t *cmd, void *data )
{ 
    // NOTE: see linux/mm/swapfile.c:sys_swapon
    L4VMblock_server_t *server = (L4VMblock_server_t *)data;
    L4VMblock_client_info_t *client;
    idl4_server_environment ipc_env;
    L4VM_server_cmd_params_t *params = (L4VM_server_cmd_params_t *)cmd->data;
    kdev_t kdev;
    struct block_device *blkdev;
    int err;
    IVMblock_handle_t handle;
    L4VMblock_conn_info_t *conn;

    ASSERT( cmd->reply_to_tid.raw );
    ASSERT( params );
    ASSERT( server->server_tid.raw );
    ipc_env._action = 0;
    kdev = MKDEV( params->attach.devid.major, params->attach.devid.minor );

    // Lookup the client.
    client = L4VMblock_client_lookup( server, params->attach.client_handle );
    if( client == NULL )
    {
	CORBA_exception_set( &ipc_env, ex_IVMblock_invalid_client, NULL );
	goto err_client;
    }

    lock_kernel();

    if( is_mounted(kdev))
    {
	CORBA_exception_set( &ipc_env, ex_IVMblock_invalid_device, NULL );
	goto err_is_mounted;
    }

    blkdev = bdget( kdev_t_to_nr(kdev) );
    if( blkdev == NULL )
    {
	CORBA_exception_set( &ipc_env, ex_IVMblock_invalid_device, NULL );
	goto err_no_device;
    }

    set_blocksize( kdev, PAGE_SIZE );
    err = blkdev_get( blkdev, 
	    ((params->attach.rw & 2) ? FMODE_READ:0) | 
	    ((params->attach.rw & 1) ? FMODE_WRITE:0), 0, 0 );
    if( err )
    {
	CORBA_exception_set( &ipc_env, ex_IVMblock_invalid_device, NULL );
	goto err_open;
    }
    set_blocksize( kdev, PAGE_SIZE );

    unlock_kernel();

    if( !L4VMblock_allocate_conn_handle(server, &handle) )
    {
	CORBA_exception_set( &ipc_env, ex_IVMblock_no_memory, NULL );
	goto err_handle;
    }
    conn = L4VMblock_conn_lookup( server, handle );
    conn->blkdev = blkdev;
    conn->block_size = block_size( kdev );
    conn->kdev = kdev;
    conn->client = client;

    dprintk( 1, KERN_INFO PREFIX "opened device.\n" );

    L4_Set_VirtualSender( server->server_tid );
    IVMblock_Control_attach_propagate_reply( cmd->reply_to_tid,
	    &handle, &ipc_env );
    return;

err_handle:
    blkdev_put( blkdev, 0 );
err_open:
err_no_device:
err_is_mounted:
    unlock_kernel();
err_client:
    printk( KERN_INFO PREFIX "failed to open a device.\n" );
    L4_Set_VirtualSender( server->server_tid );
    IVMblock_Control_attach_propagate_reply( cmd->reply_to_tid, NULL, &ipc_env);
}

static void
L4VMblock_detach_handler( L4VM_server_cmd_t *cmd, void *data )
{ 
    L4VMblock_server_t *server = (L4VMblock_server_t *)data;
    idl4_server_environment ipc_env;
    L4VM_server_cmd_params_t *params = (L4VM_server_cmd_params_t *)cmd->data;
    L4VMblock_conn_info_t *conn;
 
    ASSERT( cmd->reply_to_tid.raw );
    ASSERT( params );
    ASSERT( server->server_tid.raw );
    ipc_env._action = 0;

    conn = L4VMblock_conn_lookup( server, params->detach.handle );
    if( conn == NULL )
    {
	CORBA_exception_set( &ipc_env, ex_IVMblock_invalid_client, NULL );
	goto out;
    }

    if( conn->blkdev )
	blkdev_put( conn->blkdev, 0 );
    conn->blkdev = NULL;
    L4VMblock_conn_release( conn );

out:
    L4_Set_VirtualSender( server->server_tid );
    IVMblock_Control_detach_propagate_reply( cmd->reply_to_tid, &ipc_env );
}

static void
L4VMblock_register_handler( L4VM_server_cmd_t *cmd, void *data )
{ 
    L4VMblock_server_t *server = (L4VMblock_server_t *)data;
    idl4_server_environment ipc_env;
    L4VM_server_cmd_params_t *params = (L4VM_server_cmd_params_t *)cmd->data;
    IVMblock_handle_t handle;
    L4VMblock_client_info_t *client;
    L4_Word_t log2size;
    int err, i;
    idl4_fpage_t idl4_fp_client, idl4_fp_server;

    ASSERT( cmd->reply_to_tid.raw );
    ASSERT( params );
    ASSERT( server->server_tid.raw );
    ipc_env._action = 0;

    // Allocate a client structure.
    if( !L4VMblock_allocate_client_handle(server, &handle) )
    {
	CORBA_exception_set( &ipc_env, ex_IVMblock_no_memory, NULL );
	goto err_handle;
    }
    client = L4VMblock_client_lookup( server, handle );

    // Get access to the client's pages.  This is a HACK.
    client->client_space = L4VM_get_client_space_info( cmd->reply_to_tid );
    if( client->client_space == NULL )
    {
	CORBA_exception_set( &ipc_env, ex_IVMblock_no_memory, NULL );
	goto err_client_space;
    }

    // Allocate a client shared info region.
    log2size = L4_SizeLog2( L4_Fpage(0,sizeof(IVMblock_client_shared_t)) );
    err = L4VM_fpage_alloc( log2size, &client->client_alloc_info );
    if( err < 0 )
    {
	printk( KERN_ERR PREFIX "unable to allocate client shared pages "
		"(size %u).\n", 1 << log2size );
	goto err_shared_alloc;
    }

    dprintk( 2, KERN_INFO PREFIX "client shared region at %p, size %ld (%d)\n",
	    (void *)L4_Address(client->client_alloc_info.fpage),
	    L4_Size(client->client_alloc_info.fpage), 
	    sizeof(IVMblock_client_shared_t) );

    // Init the client shared page.
    client->client_shared = (IVMblock_client_shared_t *)
	L4_Address( client->client_alloc_info.fpage );
    client->client_shared->server_irq_no = L4VMblock_irq_no;
    client->client_shared->server_irq_tid = server->irq.my_irq_tid;
    client->client_shared->client_irq_status = 0;
    client->client_shared->client_irq_pending = 0;
    for( i = 0; i < IVMblock_descriptor_ring_size; i++ )
    {
	client->client_shared->desc_ring[i].page = NULL;
	client->client_shared->desc_ring[i].status.raw = 0;
    }

    client->ring_info.start_free = 0;
    client->ring_info.start_dirty = 0;
    client->ring_info.cnt = IVMblock_descriptor_ring_size;
    for( i = 0; i < client->ring_info.cnt; i++ )
	client->bh_ring[i] = &client->bh_ring_storage[i];

    // Build the reply fpages.
    idl4_fpage_set_base( &idl4_fp_client, 0 );
    idl4_fpage_set_mode( &idl4_fp_client, IDL4_MODE_MAP );
    idl4_fpage_set_page( &idl4_fp_client, client->client_alloc_info.fpage );
    idl4_fpage_set_permissions( &idl4_fp_client, 
	    IDL4_PERM_WRITE | IDL4_PERM_READ );

    // The server page is mapped after the client page.
    idl4_fpage_set_base( &idl4_fp_server, 
	    L4_Size(client->client_alloc_info.fpage) );
    idl4_fpage_set_mode( &idl4_fp_server, IDL4_MODE_MAP );
    idl4_fpage_set_page( &idl4_fp_server, server->server_alloc_info.fpage );
    idl4_fpage_set_permissions( &idl4_fp_server, 
	    IDL4_PERM_WRITE | IDL4_PERM_READ );

    // Reply to the client.
    L4_Set_VirtualSender( server->server_tid );
    IVMblock_Control_register_propagate_reply( cmd->reply_to_tid, 
	    &handle, &idl4_fp_client, &idl4_fp_server, &ipc_env );
    return;

err_shared_alloc:
    L4VMblock_client_release( client );
err_client_space:
err_handle:
    L4_Set_VirtualSender( server->server_tid );
    IVMblock_Control_register_propagate_reply( cmd->reply_to_tid, 
	    0, NULL, NULL, &ipc_env );
}

/***************************************************************************
 *
 * Linux interrupt functions.
 *
 ***************************************************************************/

static void
L4VMblock_bottom_half_handler( void *data )
{
    L4VMblock_server_t *server = (L4VMblock_server_t *)data;

    ASSERT( !in_interrupt() );

    L4VM_server_cmd_dispatcher( &server->bottom_half_cmds, server,
	    server->server_tid );
}

#if !defined(L4VMBLOCK_DO_IRQ_DISPATCH)
static void
L4VMblock_dispatch_handler( void *data )
{
    L4VMblock_server_t *server = (L4VMblock_server_t *)data;

    L4VMblock_process_client_io( server );
}
#endif

static void
L4VMblock_irq_handler( int irq, void *data, struct pt_regs *regs )
{
    L4VMblock_server_t *server = (L4VMblock_server_t *)data;

    while( 1 )
    {
	// Outstanding events? Read them and reset without losing events.
	L4_Word_t events = L4VM_irq_status_reset( &server->irq.status );
	L4_Word_t client_events = L4VM_irq_status_reset( &server->server_info->irq_status);
	if( !events && !client_events )
	    return;

	dprintk( 3, KERN_INFO PREFIX "irq handler: 0x%lx/0x%lx\n", 
		events, client_events );

	server->irq.pending = TRUE;
	server->server_info->irq_pending = TRUE;

	if( client_events )
	{
#if defined(L4VMBLOCK_DO_IRQ_DISPATCH)
	    L4VMblock_process_client_io( server );
#else
	    schedule_task( &server->dispatch_task );
#endif
	}

	if( (events & L4VMBLOCK_IRQ_TOP_HALF_CMD) != 0 )
	    L4VM_server_cmd_dispatcher( &server->top_half_cmds, server,
		    server->server_tid );

	if( (events & L4VMBLOCK_IRQ_BOTTOM_HALF_CMD) != 0 )
	{
	    // Make sure that we tackle bottom halves outside the
	    // interrupt context!!
	    schedule_task( &server->bottom_half_task );
	}

	// Enable interrupt message delivery.
	server->irq.pending = FALSE;
	server->server_info->irq_pending = FALSE;
    }
}

static int
L4VMblock_irq_pending( void *data )
{
    L4VMblock_server_t *server = (L4VMblock_server_t *)data;

    return (server->irq.status & server->irq.mask) ||
	server->server_info->irq_status;
}

/***************************************************************************
 *
 * Server thread dispatch.
 *
 ***************************************************************************/

IDL4_INLINE void IVMblock_Control_probe_implementation(
	CORBA_Object _caller,
	const IVMblock_devid_t *devid,
	IVMblock_devprobe_t *probe_data,
	idl4_server_environment *_env)
{
    L4VMblock_server_t *server = &L4VMblock_server;
    L4_Word16_t cmd_idx;
    L4VM_server_cmd_t *cmd;
    L4VM_server_cmd_params_t *params;

    dprintk( 3, KERN_INFO PREFIX "server thread: client probe request\n" );

    cmd_idx = L4VM_server_cmd_allocate( &server->bottom_half_cmds,
	    L4VMBLOCK_IRQ_BOTTOM_HALF_CMD, &server->irq,
	    L4VMblock_irq_no );
    cmd = &server->bottom_half_cmds.cmds[ cmd_idx ];
    params = &server->bottom_half_params[ cmd_idx ];

    params->probe.devid = *devid;
    cmd->reply_to_tid = _caller;
    cmd->data = params;
    cmd->handler = L4VMblock_probe_handler;

    L4VM_irq_deliver( &server->irq, L4VMblock_irq_no, 
	    L4VMBLOCK_IRQ_BOTTOM_HALF_CMD );

    idl4_set_no_response( _env );  // Handle the request in a safe context.
}
IDL4_PUBLISH_IVMBLOCK_CONTROL_PROBE(IVMblock_Control_probe_implementation);


IDL4_INLINE void IVMblock_Control_attach_implementation(
	CORBA_Object _caller,
	const IVMblock_handle_t client_handle,
	const IVMblock_devid_t *devid,
	const unsigned int rw,
	IVMblock_handle_t *handle,
	idl4_server_environment *_env)
{
    L4VMblock_server_t *server = &L4VMblock_server;
    L4_Word16_t cmd_idx;
    L4VM_server_cmd_t *cmd;
    L4VM_server_cmd_params_t *params;

    dprintk( 3, KERN_INFO PREFIX "server thread: client attach request\n" );

    cmd_idx = L4VM_server_cmd_allocate( &server->bottom_half_cmds,
	    L4VMBLOCK_IRQ_BOTTOM_HALF_CMD, &server->irq,
	    L4VMblock_irq_no );
    cmd = &server->bottom_half_cmds.cmds[ cmd_idx ];
    params = &server->bottom_half_params[ cmd_idx ];

    params->attach.client_handle = client_handle;
    params->attach.devid = *devid;
    params->attach.rw = rw;
    cmd->reply_to_tid = _caller;
    cmd->data = params;
    cmd->handler = L4VMblock_attach_handler;

    L4VM_irq_deliver( &server->irq, L4VMblock_irq_no, 
	    L4VMBLOCK_IRQ_BOTTOM_HALF_CMD );

    idl4_set_no_response( _env );  // Handle the request in a safe context.
}
IDL4_PUBLISH_IVMBLOCK_CONTROL_ATTACH(IVMblock_Control_attach_implementation);


IDL4_INLINE void IVMblock_Control_detach_implementation(
	CORBA_Object _caller,

	const IVMblock_handle_t handle,
	idl4_server_environment *_env)
{
    L4VMblock_server_t *server = &L4VMblock_server;
    L4_Word16_t cmd_idx;
    L4VM_server_cmd_t *cmd;
    L4VM_server_cmd_params_t *params;

    dprintk( 3, KERN_INFO PREFIX "server thread: client detach request\n" );

    cmd_idx = L4VM_server_cmd_allocate( &server->bottom_half_cmds,
	    L4VMBLOCK_IRQ_BOTTOM_HALF_CMD, &server->irq,
	    L4VMblock_irq_no );
    cmd = &server->bottom_half_cmds.cmds[ cmd_idx ];
    params = &server->bottom_half_params[ cmd_idx ];

    params->detach.handle = handle;
    cmd->reply_to_tid = _caller;
    cmd->data = params;
    cmd->handler = L4VMblock_detach_handler;

    L4VM_irq_deliver( &server->irq, L4VMblock_irq_no, 
	    L4VMBLOCK_IRQ_BOTTOM_HALF_CMD );

    idl4_set_no_response( _env );  // Handle the request in a safe context.
}
IDL4_PUBLISH_IVMBLOCK_CONTROL_DETACH(IVMblock_Control_detach_implementation);


IDL4_INLINE void IVMblock_Control_register_implementation(
	CORBA_Object _caller,
	IVMblock_handle_t *client_handle,
	idl4_fpage_t *client_config,
	idl4_fpage_t *server_config,
	idl4_server_environment *_env)
{
    L4VMblock_server_t *server = &L4VMblock_server;
    L4_Word16_t cmd_idx;
    L4VM_server_cmd_t *cmd;
    L4VM_server_cmd_params_t *params;

    dprintk( 3, KERN_INFO PREFIX "server thread: client register request\n" );

    cmd_idx = L4VM_server_cmd_allocate( &server->bottom_half_cmds,
	    L4VMBLOCK_IRQ_BOTTOM_HALF_CMD, &server->irq,
	    L4VMblock_irq_no );
    cmd = &server->bottom_half_cmds.cmds[ cmd_idx ];
    params = &server->bottom_half_params[ cmd_idx ];

    cmd->reply_to_tid = _caller;
    cmd->data = params;
    cmd->handler = L4VMblock_register_handler;

    L4VM_irq_deliver( &server->irq, L4VMblock_irq_no, 
	    L4VMBLOCK_IRQ_BOTTOM_HALF_CMD );

    idl4_set_no_response( _env );  // Handle the request in a safe context.
}
IDL4_PUBLISH_IVMBLOCK_CONTROL_REGISTER(IVMblock_Control_register_implementation);


static void
L4VMblock_server_thread( void *data )
{
    L4_ThreadId_t partner;
    L4_MsgTag_t msgtag;
    idl4_msgbuf_t msgbuf;
    long cnt;
    static void *IVMblock_Control_vtable[IVMBLOCK_CONTROL_DEFAULT_VTABLE_SIZE] = IVMBLOCK_CONTROL_DEFAULT_VTABLE;

    idl4_msgbuf_init(&msgbuf);

    while (1)
    {
    	partner = L4_nilthread;
      	msgtag.raw = 0;
	cnt = 0;

	while (1)
	{
	    idl4_msgbuf_sync(&msgbuf);

	    idl4_reply_and_wait(&partner, &msgtag, &msgbuf, &cnt);

	    if (idl4_is_error(&msgtag))
		break;

	    idl4_process_request(&partner, &msgtag, &msgbuf, &cnt, IVMblock_Control_vtable[idl4_get_function_id(&msgtag) & IVMBLOCK_CONTROL_FID_MASK]);
	}
    }
}

void IVMblock_Control_discard(void)
{
    // Invoked in response to invalid IPC requests.
}

/***************************************************************************
 *
 * Linux module functions.
 *
 ***************************************************************************/

static int __init
L4VMblock_server_init_module( void )
{
    int err, i;
    L4VMblock_server_t *server = &L4VMblock_server;
    L4_Word_t log2size;

    L4VM_irq_init( &server->irq );
    L4VM_server_cmd_init( &server->top_half_cmds );
    L4VM_server_cmd_init( &server->bottom_half_cmds );

    for( i = 0; i < L4VMBLOCK_MAX_CLIENTS; i++ )
	server->clients[i].handle = L4VMBLOCK_INVALID_HANDLE;
    for( i = 0; i < L4VMBLOCK_MAX_DEVICES; i++ )
	server->connections[i].handle = L4VMBLOCK_INVALID_HANDLE;

    INIT_TQUEUE( &server->bottom_half_task, 
	    L4VMblock_bottom_half_handler, server );
#if !defined(L4VMBLOCK_DO_IRQ_DISPATCH)
    INIT_TQUEUE( &server->dispatch_task,
	    L4VMblock_dispatch_handler, server );
#endif

    // Allocate a shared memory region for access by all clients.
    log2size = L4_SizeLog2( L4_Fpage(0,sizeof(IVMblock_server_shared_t)) );
    err = L4VM_fpage_alloc( log2size, &server->server_alloc_info );
    if( err < 0 )
    {
	printk( KERN_ERR PREFIX "unable to allocate server shared pages "
		"(size %u).\n", 1 << log2size );
	goto err_shared_alloc;
    }
    server->server_info = (IVMblock_server_shared_t *)
	L4_Address( server->server_alloc_info.fpage );
    server->server_info->irq_status = 0;
    server->server_info->irq_pending = 0;

    // Register an IRQ handler with Linux.
    err = request_mlx_irq( L4_nilthread, L4VMblock_irq_handler,
	    SA_SHIRQ, "L4VMblock", server, L4VMblock_irq_pending,
	    server, &server->irq.my_irq_tid );
    if( err >= 0 )
	L4VMblock_irq_no = err;
    if( err < 0 )
    {
	printk( KERN_ERR PREFIX "unable to allocate an interrupt.\n" );
	goto err_request_irq;
    }

    // Start the server thread.
    server->server_tid = l4lx_thread_create( L4VMblock_server_thread,
	    NULL, server, sizeof(server), PRIO_IRQ(0), 
	    "L4VMblock server thread" );
    if( L4_IsNilThread(server->server_tid) )
    {
	printk( KERN_ERR PREFIX "failed to start the server thread.\n" );
	err = -ENOMEM;
	goto err_thread_start;
    }

    // Register with the locator.
    err = L4VM_server_register_location( UUID_IVMblock, server->server_tid );
    if( err == -ENODEV )
	printk( KERN_ERR PREFIX "failed to register with the locator.\n" );
    else if( err )
	printk( KERN_ERR PREFIX "failed to contact the locator service.\n" );

    printk( KERN_INFO PREFIX "L4VMblock driver initialized.\n" );

    return 0;

err_thread_start:
    // TODO: free the interrupt handler
err_request_irq:
    L4VM_fpage_dealloc( &server->server_alloc_info );
err_shared_alloc:
    return err;
}

static void __exit
L4VMblock_server_exit_module( void )
{
#if 0
    L4VMblock_server_t *server = &L4VMblock_server;
    if( !L4_IsNilThread(server->server_tid) )
    {
	// TODO: free the interrupt handler
	l4lx_thread_shutdown( server->server_tid );
	server->server_tid = L4_nilthread;
    }
#endif
}

MODULE_AUTHOR( "Joshua LeVasseur <jtl@bothan.net>" );
MODULE_DESCRIPTION( "L4Linux block driver server" );
MODULE_LICENSE( "Proprietary, owned by Joshua LeVasseur" );
MODULE_SUPPORTED_DEVICE( "L4 VM block" );

#if defined(MODULE)
module_init( L4VMblock_server_init_module );
module_exit( L4VMblock_server_exit_module );
#else
void __init l4vm_block_init( void ) { L4VMblock_server_init_module(); }
#endif
