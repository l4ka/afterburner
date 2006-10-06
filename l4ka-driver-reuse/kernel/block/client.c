/*********************************************************************
 *                
 * Copyright (C) 2004 Joshua LeVasseur
 *
 * File path:	linuxblock/L4VMblock_client.c
 * Description:	Implements the client stub portion of a virtual 
 * 		block device for L4Linux.
 *
 * Proprietary!  DO NOT DISTRIBUTE!
 *
 * $Id: client.c,v 1.1 2006/09/21 09:28:35 joshua Exp $
 *                
 ********************************************************************/

/*
 * The logic for handling the request queue is modeled from 
 * linux/drivers/block/nbd.c.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/blkpg.h>
#include <linux/devfs_fs_kernel.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/l4lxapi/thread.h>
#include <asm/l4linux/debug.h>

/*********
 * begin blk.h stuff
 */
#define MAJOR_NR	L4VMblock_major
static int L4VMblock_major;	/* Must be declared before including blk.h. */
#define DEVICE_NR(d)	MINOR(d)	/* No partition bits. */
#define DEVICE_NAME	"vmblock"
#define DEVICE_NO_RANDOM
#define DEVICE_OFF(d)	/* do nothing */
#define DEVICE_REQUEST	L4VMblock_client_request
#define LOCAL_END_REQUEST

#include <linux/blk.h>
/*
 * end blk.h stuff
 *********/

#define PREFIX "L4VMblock client: "

#include "client.h"



/*
 * How do we virtualize devices?  The client requests a connection to a
 * block device in the server.  The client identifies a server block device
 * via a major and minor tuple.  If the server accepts the connection,
 * then the client creates a new block request queue within its Linux.
 *
 * How does the client Linux know the available major and minor numbers?
 * Since the client Linux has no real device drivers, it can claim all the
 * official block device numbers, and just forward requests to the server.
 * But the client must reserve the appropriate major/minor tuples.  It can
 * do this by asking the server for all valid tuples, and then replicating
 * them in the client space.  The server need not provide access to all of
 * its devices; it especially must not if the devices are already in use.
 * What is the definition of in-use?  For r/w disks, a particular partition.
 *
 * Block device sequencing:
 * 1.  Probe(major,minor): client requests information about a static device.
 *     The client can't perform operations, but it can obtain the information
 *     necessary to create a virtual Linux device, such as the block size,
 *     read-ahead, etc.  The server can permit a device probe even
 *     if it won't permit the client to open the device.
 * 2.  Open(major,minor): the client obtains permission to operate on the
 *     device.  The server puts the device into a state where it is
 *     functional.
 * 3.  Request(major,minor): the client requests a block read/write operation.
 * 4.  Close(major,minor): the client terminates its device session.
 */

int L4VMblock_probe_devices[16] = { 3, 1, 3, 2, 3, 3 };
MODULE_PARM( L4VMblock_probe_devices, "0-16i" );
#define L4VMBLOCK_MAX_PROBE_PARAMS	\
	(sizeof(L4VMblock_probe_devices)/sizeof(L4VMblock_probe_devices[0]))

#if !defined(CONFIG_AFTERBURN_DRIVERS_BLOCK_OPTIMIZE)
int L4VMblock_debug_level = 2;
MODULE_PARM( L4VMblock_debug_level, "i" );
#endif

static L4_Word_t L4VMblock_irq_no;
static L4VMblock_client_t L4VMblock_client;

#define L4VMBLOCK_MAX_DEVS	16

static int L4VMblock_dev_length[ L4VMBLOCK_MAX_DEVS ];
static int L4VMblock_hardsect_sizes[ L4VMBLOCK_MAX_DEVS ];
static int L4VMblock_block_sizes[ L4VMBLOCK_MAX_DEVS ];
static int L4VMblock_max_request_sectors[ L4VMBLOCK_MAX_DEVS ];

static devfs_handle_t devfs_handle;

static L4VMblock_descriptor_t L4VMblock_descriptors[ L4VMBLOCK_MAX_DEVS ];

static int L4VMblock_probe_device( L4VMblock_client_t *, kdev_t, int minor );
static int L4VMblock_attach_device( L4VMblock_client_t *, L4VMblock_descriptor_t *);
static int L4VMblock_detach_device( L4VMblock_client_t *, L4VMblock_descriptor_t *);
static void L4VMblock_deliver_server_irq( L4VMblock_client_t *client );

/****************************************************************************
 *
 * Functions for providing the device driver's block interface.
 *
 ****************************************************************************/

static int L4VMblock_open( struct inode *inode, struct file *file )
{
    int err;

    L4VMblock_client_t *client = &L4VMblock_client;
    int minor = DEVICE_NR( inode->i_rdev );
    L4VMblock_descriptor_t *descriptor;

    if( minor >= L4VMBLOCK_MAX_DEVS )
	return -ENXIO;

    descriptor = &L4VMblock_descriptors[minor];
    if( descriptor->refcnt == 0 )
    {
	err = L4VMblock_attach_device( client, descriptor );
	if( err == 0 )
    	    descriptor->refcnt++;
    }
    else
    {
	err = 0;
	descriptor->refcnt++;
    }

    return err;
}

static int L4VMblock_release( struct inode *inode, struct file *file )
{
    L4VMblock_client_t *client = &L4VMblock_client;
    int minor = DEVICE_NR( inode->i_rdev );
    L4VMblock_descriptor_t *descriptor;

    if( minor >= L4VMBLOCK_MAX_DEVS )
	return -ENXIO;

    descriptor = &L4VMblock_descriptors[minor];

    if( descriptor->refcnt == 0 )
	return 0;
    descriptor->refcnt--;
    if( descriptor->refcnt )
	return 0;

    return L4VMblock_detach_device( client, descriptor );
}

static int L4VMblock_ioctl( struct inode *inode, struct file * file,
	unsigned cmd, unsigned long opt )
{
    unsigned long size;
    int err;

    switch( cmd )
    {
	case BLKGETSIZE:
	    // Be careful, don't lose bits!
	    size = L4VMblock_dev_length[MINOR(inode->i_rdev)];
	    size *= 
		(BLOCK_SIZE / L4VMblock_hardsect_sizes[MINOR(inode->i_rdev)]);
	    err = put_user( size, (unsigned long *)opt );
	    dprintk( 2, KERN_INFO PREFIX "ioctl(BLKGETSIZE) --> %lu\n", size );
	    return err;

	case BLKGETSIZE64:
	    size = 
		(L4VMblock_dev_length[MINOR(inode->i_rdev)] << BLOCK_SIZE_BITS);
	    err = put_user( size, (unsigned long *)opt );
	    dprintk( 2, KERN_INFO PREFIX "ioctl(BLKGETSIZE64) --> %lu\n", size);
	    return err;

	default:
	    err = blk_ioctl( inode->i_rdev, cmd, opt );
	    dprintk( 2, KERN_INFO PREFIX "ioctl(%d) returns %d\n",
		    cmd, err );
	    return err;
    }
}

static int L4VMblock_check_media_change( kdev_t kdev )
{
    // Returns 1 if the media changed, 0 otherwise.
    return 0;
}

static int L4VMblock_revalidate( kdev_t kdev )
{
    return 0;
}

static struct block_device_operations L4VMblock_device_operations = {
    open: L4VMblock_open,
    release: L4VMblock_release,
    ioctl: L4VMblock_ioctl,
    check_media_change: L4VMblock_check_media_change,
    revalidate: L4VMblock_revalidate
};

static L4VMblock_request_shadow_t *
L4VMblock_new_request_shadow( L4VMblock_client_t *client, struct request *req )
{
    L4VMblock_request_shadow_t *req_shadow;

    req_shadow = kmalloc( sizeof(L4VMblock_request_shadow_t), GFP_ATOMIC );
    if( req_shadow == NULL )
	return NULL;

    req_shadow->request = req;
    req_shadow->bh_count = 0;
    INIT_LIST_HEAD( &req_shadow->bh_list );

    // The io_request_lock must be held when manipulating the linked list.
    list_add_tail( &req_shadow->shadow_list, &client->shadow_list );

    return req_shadow;
}

static void 
L4VMblock_walk_request( L4VMblock_client_t *client, struct request *req )
    // The io_request_lock must *not* be held, since this function
    // may block.
{
    struct buffer_head *bh;
    L4VMblock_descriptor_t *descriptor = 
	&L4VMblock_descriptors[MINOR(req->rq_dev)];
    volatile IVMblock_ring_descriptor_t *rdesc;
    L4VMblock_ring_t *ring_info = &client->desc_ring_info;
    int nsect;
    L4VMblock_request_shadow_t *req_shadow;
    L4VMblock_bh_shadow_t *bh_shadow;

    if( !descriptor->refcnt )
    {
	dprintk( 2, KERN_INFO PREFIX "device not attached: %x:%x.\n",
		MAJOR(req->rq_dev), MINOR(req->rq_dev) );
	req->errors++;
	return;
    }

    ASSERT(req->bh); // The request must have at least one buffer head.

    // Init the shadow data structures.
    req_shadow = L4VMblock_new_request_shadow( client, req );
    if( req_shadow == NULL )
    {
	req->errors++;
	return;
    }

    // Tentatively queue the buffer heads.
    while( req->bh )
    {
	// Wait for an available descriptor.
	while( L4VMblock_ring_available(ring_info) == 0 )
	{
	    dprintk( 4, PREFIX "queue full, sleeping.\n" );
	    spin_unlock_irq(&io_request_lock);
	    L4VMblock_deliver_server_irq( client );
	    wait_event( client->ring_wait, 
		    (L4VMblock_ring_available(ring_info) > 0) );
	    spin_lock_irq(&io_request_lock);
	}

	// Get the next descriptor.
	rdesc = &client->client_shared->desc_ring[ ring_info->start_free ];
	ASSERT( !rdesc->status.X.server_owned );

	// Create a bh shadow.
	bh_shadow = kmalloc( sizeof(L4VMblock_bh_shadow_t), GFP_ATOMIC );
	ASSERT( bh_shadow );
	if( bh_shadow == NULL )
	{
	    req->errors++;
	    return;
	}

	// Remove the buffer head from the request.
	bh = req->bh;
	req->bh = bh->b_reqnext;
	bh->b_reqnext = NULL;

	nsect = bh->b_size >> 9;
    	req->hard_sector += nsect;
	req->sector += nsect;
	req->hard_nr_sectors -= nsect;
	req->nr_sectors -= nsect;

	if( req->bh )
	{
	    req->current_nr_sectors = req->bh->b_size >> 9;
	    req->hard_cur_sectors = req->current_nr_sectors;
	    if( req->nr_sectors < req->current_nr_sectors )
	    {
		req->nr_sectors = req->current_nr_sectors;
		printk( KERN_ERR PREFIX "end_request: buffer-list destroyed\n");
	    }
	    req->buffer = req->bh->b_data;
	}
	else
	    blkdev_dequeue_request( req );

	// Init the bh shadow.
	bh_shadow->bh = bh;
	bh_shadow->owner = req_shadow;
	// The io_request_lock must be held while manipulating the linked list
	// and the bh_count.
	req_shadow->bh_count++;
	list_add_tail( &bh_shadow->bh_list, &req_shadow->bh_list );

	// Fill-in the descriptor.
	rdesc->handle = descriptor->handle;
	rdesc->size = bh->b_size;
	rdesc->offset = bh->b_rsector;
	rdesc->page = (void *)(virt_to_bus(bh->b_data));
	rdesc->status.raw = 0;
	rdesc->status.X.do_write = (req->cmd == WRITE);
	rdesc->status.X.speculative = (req->cmd == READA);
	rdesc->client_data = (void *)bh_shadow;

	dprintk( 4, KERN_INFO PREFIX "offset %p, size %p\n",
		(void *)rdesc->offset, (void *)rdesc->size );

	// Transfer the buffers to the server.
	rdesc->status.X.server_owned = 1;

	// Next ...
	ring_info->start_free = (ring_info->start_free + 1) % ring_info->cnt;
    }
}

static void 
L4VMblock_end_request( L4VMblock_client_t *client, struct request *req )
    // Must be called while the io_request_lock is held.
{
    int uptodate = (req->errors == 0);

    dprintk( 2, KERN_INFO PREFIX "prematurely ending block request.\n" );

    if( !end_that_request_first(req, uptodate, "vmblock") )
    {
	blkdev_dequeue_request( req );
	end_that_request_last( req );
    }
}

static void
L4VMblock_client_request( request_queue_t *queue )
{
    L4VMblock_client_t *client = &L4VMblock_client;
    struct request *req;

    if( client->rings_busy )
    {
	dprintk( 2, PREFIX "reentrance denied.\n" );
	return;
    }
    client->rings_busy = 1;

    dprintk( 4, PREFIX "request submitted.\n" );

    while( !QUEUE_EMPTY )
    {
	req = CURRENT;
	ASSERT(req);
	req->errors = 0;
	if( req->bh == NULL )
	    L4VMblock_end_request( client, req );
	else
	{
	    L4VMblock_walk_request( client, req );
	    if( req->errors )
	       	L4VMblock_end_request( client, req );
	}
    }

    client->rings_busy = 0;

    spin_unlock_irq(&io_request_lock);
    L4VMblock_deliver_server_irq( client );
    spin_lock_irq(&io_request_lock);
}

/****************************************************************************
 *
 * Functions for handling the server.
 *
 ****************************************************************************/

static int L4VMblock_probe_device( 
	L4VMblock_client_t *client,
	kdev_t kdev,
	int minor )
{
    CORBA_Environment ipc_env;
    unsigned long irq_flags;
    IVMblock_devprobe_t probe_data;
    IVMblock_devid_t devid = { major: MAJOR(kdev), minor: MINOR(kdev) };
    char name[10];

    ASSERT( devfs_handle );

    ipc_env = idl4_default_environment;
    local_irq_save(irq_flags);
    IVMblock_Control_probe( client->server_tid, &devid, &probe_data, &ipc_env );
    local_irq_restore(irq_flags);

    if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free(&ipc_env);
	printk( KERN_WARNING PREFIX "failed to probe device %d:%d\n",
		MAJOR(kdev), MINOR(kdev) );
	return -ENODEV;
    }

    dprintk( 1, KERN_INFO PREFIX "probed remote device %d:%d\n", 
	    MAJOR(kdev), MINOR(kdev) );
    dprintk( 2, KERN_INFO PREFIX "device size: %lu KB\n"
	    "\tblock size: %lu\n"
	    "\thardware sector size: %lu\n"
	    "\tread ahead: %lu\n"
	    "\tmax read ahead: %lu\n"
	    "\trequest max sectors: %lu\n",
	    probe_data.device_size, probe_data.block_size,
	    probe_data.hardsect_size, probe_data.read_ahead,
	    probe_data.max_read_ahead, probe_data.req_max_sectors );

    L4VMblock_dev_length[ minor ] = probe_data.device_size;
    L4VMblock_hardsect_sizes[ minor ] = probe_data.hardsect_size;
    L4VMblock_block_sizes[ minor ] = PAGE_SIZE;
    L4VMblock_max_request_sectors[ minor ] = probe_data.req_max_sectors;

    L4VMblock_descriptors[ minor ].remote_kdev = kdev;
    L4VMblock_descriptors[ minor ].refcnt = 0;

    snprintf( name, sizeof(name), "%u", minor );
    devfs_register( devfs_handle, name, DEVFS_FL_DEFAULT, MAJOR_NR, minor, 
	    S_IFBLK | S_IRUSR | S_IWUSR, &L4VMblock_device_operations, client );

    return 0;
}

static int L4VMblock_attach_device( 
	L4VMblock_client_t *client,
	L4VMblock_descriptor_t *descriptor )
{
    CORBA_Environment ipc_env;
    unsigned long irq_flags;
    IVMblock_devid_t devid;

    devid.major = MAJOR( descriptor->remote_kdev );
    devid.minor = MINOR( descriptor->remote_kdev );

    ipc_env = idl4_default_environment;
    local_irq_save(irq_flags);
    IVMblock_Control_attach( client->server_tid, client->handle, &devid, 3, 
	    &descriptor->handle, &ipc_env );
    local_irq_restore(irq_flags);

    if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free(&ipc_env);
	printk( KERN_WARNING PREFIX "failed to open device %d:%d\n",
	    	MAJOR(descriptor->remote_kdev), MINOR(descriptor->remote_kdev));
	return -EBUSY;
    }

    dprintk( 2, KERN_INFO PREFIX "opened %d:%d\n", 
	    MAJOR(descriptor->remote_kdev), MINOR(descriptor->remote_kdev) );

    return 0;
}

static int L4VMblock_detach_device( 
	L4VMblock_client_t *client,
	L4VMblock_descriptor_t *descriptor )
{
    CORBA_Environment ipc_env;
    unsigned long irq_flags;

    ipc_env = idl4_default_environment;
    local_irq_save(irq_flags);
    IVMblock_Control_detach( client->server_tid, descriptor->handle, &ipc_env );
    local_irq_restore(irq_flags);

    if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free(&ipc_env);
	printk( KERN_WARNING PREFIX "failed to close device %d:%d\n",
	    	MAJOR(descriptor->remote_kdev), MINOR(descriptor->remote_kdev));
	return -EBUSY;
    }

    dprintk( 2, KERN_INFO PREFIX "closed %d:%d\n", 
	    MAJOR(descriptor->remote_kdev), MINOR(descriptor->remote_kdev) );

    return 0;
}

static void
L4VMblock_deliver_server_irq( L4VMblock_client_t *client )
{
    CORBA_Environment ipc_env = idl4_default_environment;

    client->server_shared->irq_status |= L4VMBLOCK_SERVER_IRQ_DISPATCH;
    if( client->server_shared->irq_pending )
	return;

    client->server_shared->irq_pending = TRUE;

    // Send a zero-timeout IPC.
    ipc_env._timeout = L4_Timeouts( L4_ZeroTime, L4_Never );

    ILinux_raise_irq( client->client_shared->server_irq_tid, 
	    client->client_shared->server_irq_no, &ipc_env );

    if( ipc_env._major != CORBA_NO_EXCEPTION )
	CORBA_exception_free( &ipc_env );
}

/****************************************************************************
 *
 * Interrupt handling functions.
 *
 ****************************************************************************/

static void
L4VMblock_finish_transfers( L4VMblock_client_t *client )
{
    L4VMblock_ring_t *ring_info = &client->desc_ring_info;
    volatile IVMblock_ring_descriptor_t *rdesc;
    struct buffer_head *bh;
    struct request *req;
    unsigned long flags;
    unsigned cleaned = 0;
    L4VMblock_bh_shadow_t *bh_shadow;
    L4VMblock_request_shadow_t *req_shadow;

    spin_lock_irqsave( &io_request_lock, flags );

    while( ring_info->start_dirty != ring_info->start_free )
    {
	// Get the next transaction.
	rdesc = &client->client_shared->desc_ring[ ring_info->start_dirty ];
	if( rdesc->status.X.server_owned )
	    break;

	cleaned++;

	// Get the shadow information.
	bh_shadow = (L4VMblock_bh_shadow_t *)rdesc->client_data;
	ASSERT( bh_shadow );
	bh = bh_shadow->bh;
	req_shadow = bh_shadow->owner;
	req = req_shadow->request;

	// Finish the transaction.
	blk_finished_io( bh->b_size >> 9 );
	blk_finished_sectors( req, bh->b_size >> 9 );
	bh->b_reqnext = NULL;

	if( rdesc->status.X.server_err )
	{
	    dprintk( 2, PREFIX "server block I/O error.\n" );
	    bh->b_end_io( bh, 0 );
	}
	else
	{
	    if( rdesc->status.X.do_write )
		mark_buffer_uptodate( bh, 1 );
	    bh->b_end_io( bh, 1 );
	}

	// Clean-up the bh shadow.
	list_del( &bh_shadow->bh_list );
	kfree( bh_shadow );

	// Update the request.
	ASSERT( req_shadow->bh_count );
	req_shadow->bh_count--;
	if( req_shadow->bh_count == 0 )
	{
	    // It was the last buffer head for this request, so 
	    // release the request.
	    end_that_request_last( req );
	    list_del( &req_shadow->shadow_list );
	    kfree( req_shadow );
	}

	// Move to the next.
	ring_info->start_dirty = (ring_info->start_dirty + 1) % ring_info->cnt;
    }

    spin_unlock_irqrestore( &io_request_lock, flags );

    if( cleaned )
	wake_up( &client->ring_wait );
}

static void
L4VMblock_irq_handler( int irq, void *data, struct pt_regs *regs )
{
    L4VMblock_client_t *client = (L4VMblock_client_t *)data;
    int cnt = 0;
    L4_Word_t events;

    do
    {
	client->client_shared->client_irq_pending = TRUE;
	events = L4VM_irq_status_reset( &client->client_shared->client_irq_status );

	if( events )
	{
	    dprintk( 3, PREFIX "irq handler: 0x%lx\n", events );
	    L4VMblock_finish_transfers( client );
	}

	client->client_shared->client_irq_pending = FALSE;
	cnt++;
	if( cnt > 100 )
	    dprintk( 1, PREFIX "too many interrupts.\n" );
    }
    while( events );
}

static int
L4VMblock_irq_pending( void *data )
{
    L4VMblock_client_t *client = (L4VMblock_client_t *)data;

    return client->client_shared->client_irq_status;
}

/****************************************************************************
 *
 * Module-level functions.
 *
 ****************************************************************************/

static int 
L4VMblock_client_register( L4VMblock_client_t *client )
{
    int err;
    L4_Word_t log2size, size, size2, window_base;
    idl4_fpage_t client_mapping, server_mapping;
    CORBA_Environment ipc_env;
    unsigned long irq_flags;

    // Try to allocate a virtual memory area for the shared windows.
    size  = L4_Size( L4_Fpage(0,sizeof(IVMblock_client_shared_t)) );
    size2 = L4_Size( L4_Fpage(0,sizeof(IVMblock_server_shared_t)) );
    log2size = L4_SizeLog2( L4_Fpage(0, size+size2) );
    err = L4VM_fpage_vmarea_get( log2size, &client->shared_window );
    if( err < 0 )
	return err;

    ASSERT( L4_Address(client->shared_window.fpage) >= 
	    (L4_Word_t)client->shared_window.vmarea->addr );

    dprintk( 2, KERN_INFO PREFIX "receive window at %p, size %ld\n",
	    (void *)L4_Address(client->shared_window.fpage),
	    L4_Size(client->shared_window.fpage) );

    // Register with the server.
    ipc_env = idl4_default_environment;
    idl4_set_rcv_window( &ipc_env, client->shared_window.fpage );
    local_irq_save(irq_flags);
    IVMblock_Control_register( client->server_tid, &client->handle,
	    &client_mapping, &server_mapping, &ipc_env );
    local_irq_restore(irq_flags);
    if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	L4VM_fpage_vmarea_release( &client->shared_window );
	return -EIO;
    }

    // Initialize shared structures.
    dprintk( 2, KERN_INFO PREFIX "client shared region at %p, offset %p, "
	    "size %ld\n",
	    (void *)idl4_fpage_get_base(client_mapping),
	    (void *)L4_Address(idl4_fpage_get_page(client_mapping)),
	    L4_Size(idl4_fpage_get_page(client_mapping)) );
    dprintk( 2, KERN_INFO PREFIX "server shared region at %p, offset %p, "
	    "size %ld\n",
	    (void *)idl4_fpage_get_base(server_mapping),
	    (void *)L4_Address(idl4_fpage_get_page(server_mapping)),
	    L4_Size(idl4_fpage_get_page(server_mapping)) );

    window_base = L4_Address( client->shared_window.fpage );
    client->client_shared = (IVMblock_client_shared_t *)
	(window_base + idl4_fpage_get_base(client_mapping));
    client->server_shared = (IVMblock_server_shared_t *)
	(window_base + idl4_fpage_get_base(server_mapping));

    dprintk( 2, KERN_INFO PREFIX "server irq tid: %p, server irq no: %lu\n",
	    RAW(client->client_shared->server_irq_tid),
	    client->client_shared->server_irq_no );

    // Initialize stuff related to the shared structures.
    client->desc_ring_info.cnt = IVMblock_descriptor_ring_size;
    client->desc_ring_info.start_free = 0;
    client->desc_ring_info.start_dirty = 0;
    client->rings_busy = FALSE;
    init_waitqueue_head( &client->ring_wait );
    INIT_LIST_HEAD( &client->shadow_list );

    return 0;
}

static int __init
L4VMblock_client_init_module( void )
{
    int i, err, minor;
    L4VMblock_client_t *client = &L4VMblock_client;

    // Locate the L4VMblock server.
    err = L4VM_server_locate( UUID_IVMblock, &client->server_tid );
    if( err == -ENODEV )
    {
	printk( KERN_ERR PREFIX "unable to locate a block service.\n" );
	goto err_no_server;
    }
    else if( err )
    {
	printk( KERN_ERR PREFIX "failed to contact the locator service.\n" );
	goto err_no_server;
    }

    // Register with the server.
    err = L4VMblock_client_register( client );
    if( err < 0 )
	goto err_register;

    // Register an IRQ handler with Linux.
    err = request_mlx_irq( L4_nilthread, L4VMblock_irq_handler,
	    SA_SHIRQ, "L4VMblock", client, L4VMblock_irq_pending,
	    client, &client->client_shared->client_irq_tid );
    if( err >= 0 )
    {
	L4VMblock_irq_no = err;
	client->client_shared->client_irq_no = L4VMblock_irq_no;
    }
    if( err < 0 )
    {
	printk( KERN_ERR PREFIX "unable to allocate an interrupt.\n" );
	goto err_request_irq;
    }

    // Register block handlers with Linux, and allocate a major number.
    err = register_blkdev( MAJOR_NR, DEVICE_NAME, &L4VMblock_device_operations);
    if( err < 0 )
    {
	printk( KERN_ERR PREFIX "unable to a block device: %d.\n", err );
	goto err_invalid_major;
    }
    else if( err == 0 )
    {
	printk( KERN_ERR PREFIX "unable to allocate a major number.\n" );
	goto err_invalid_major;
    }

    MAJOR_NR = err;
    dprintk( 1, KERN_INFO PREFIX "major number %d.\n", MAJOR_NR );

    // Finish the block driver init.
    hardsect_size[MAJOR_NR] = L4VMblock_hardsect_sizes;
    blksize_size[MAJOR_NR] = L4VMblock_block_sizes;
    blk_size[MAJOR_NR] = L4VMblock_dev_length;
    max_sectors[MAJOR_NR] = L4VMblock_max_request_sectors;

    blk_init_queue( BLK_DEFAULT_QUEUE(MAJOR_NR), DEVICE_REQUEST );
    blk_queue_headactive( BLK_DEFAULT_QUEUE(MAJOR_NR), 0 );

    // Create a device tree in the devfs.
    devfs_handle = devfs_mk_dir( NULL, "vmblock", NULL );

    // Probe for some devices within the server.  These probes are based
    // on the module parameters L4VMblock_probe_devices.
    for( i = 0, minor = 0; 
	    (i < L4VMBLOCK_MAX_PROBE_PARAMS) && (minor < L4VMBLOCK_MAX_DEVS);
	    i += 2 )
    {
	L4_Word_t target_major = L4VMblock_probe_devices[i];
	L4_Word_t target_minor = L4VMblock_probe_devices[i+1];
	if( target_major == 0 )
	    break;
	if( L4VMblock_probe_device(client, MKDEV(target_major,target_minor), minor) == 0 )
	    minor++;
    }

    return 0;

err_invalid_major:
err_request_irq:
err_register:
err_no_server:
    return err;
}

static void __exit
L4VMblock_client_exit_module( void )
{
    // TODO: unregister the irq handler.

    if( devfs_handle )
	devfs_unregister( devfs_handle );
    devfs_handle = NULL;

    hardsect_size[MAJOR_NR] = NULL;
    blksize_size[MAJOR_NR] = NULL;
    blk_size[MAJOR_NR] = NULL;

    MAJOR_NR = 0;
}

MODULE_AUTHOR( "Joshua LeVasseur <jtl@bothan.net>" );
MODULE_DESCRIPTION( "L4Linux client stub block driver" );
MODULE_LICENSE( "Proprietary, owned by Joshua LeVasseur" );
MODULE_SUPPORTED_DEVICE( "L4 VM block" );

#if defined(MODULE)
module_init( L4VMblock_client_init_module );
module_exit( L4VMblock_client_exit_module ); 
#else
void __init l4vm_block_init( void ) { L4VMblock_client_init_module(); }

static int __init L4VMblock_setup_probe_param( char *param )
    // Parses a kernel command line parameter, consisting of a series
    // of major,minor pairs.  An example such parameter:
    //   bootprobe=3,1:3,2:3,3
{
    int idx = 0;
    int major, minor;

    while( idx < L4VMBLOCK_MAX_PROBE_PARAMS-2 )
    {
	if( !*param )
	    break;

	major = simple_strtoul(param, &param, 10);
	if( !*param || !*(++param) ) // Skip the separator.
	    break;
	minor = simple_strtoul(param, &param, 10);

	L4VMblock_probe_devices[idx++] = major;
	L4VMblock_probe_devices[idx++] = minor;

	if( *param )	// Skip the separator.
	    param++;
    }
    if( *param )
	printk( KERN_WARNING PREFIX "too many block probes.\n" );
    L4VMblock_probe_devices[idx] = 0;
    return 1;
}
__setup( "blockprobe=", L4VMblock_setup_probe_param );
#endif

