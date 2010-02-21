/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/block/client26.c
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
#include <linux/blkdev.h>
#include <linux/interrupt.h>

#include <asm/io.h>

#include "client.h"

#if defined(CONFIG_X86_IO_APIC)
#include <acpi/acpi.h>
#include <linux/acpi.h>
#endif

#undef PREFIX

#define PREFIX "L4VMblock client: "

static int L4VMblock_major;

int L4VMblock_probe_devices[16] = { 0, 0 };
MODULE_PARM( L4VMblock_probe_devices, "0-16i" );
#define L4VMBLOCK_MAX_PROBE_PARAMS	\
	(sizeof(L4VMblock_probe_devices)/sizeof(L4VMblock_probe_devices[0]))

#if !defined(CONFIG_AFTERBURN_DRIVERS_BLOCK_OPTIMIZE)
int L4VMblock_debug_level = 2;
MODULE_PARM( L4VMblock_debug_level, "i" );
#endif

int L4VMblock_irq = 0;
MODULE_PARM( L4Vblock_irq, "i" );

static L4VMblock_client_t L4VMblock_client;

#define L4VMBLOCK_MAX_DEVS	16

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
    L4VMblock_descriptor_t *descriptor = 
	(L4VMblock_descriptor_t *)inode->i_bdev->bd_disk->private_data;
    struct block_device *bdev = inode->i_bdev;

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

    set_blocksize( bdev, PAGE_SIZE );

    return err;
}

static int L4VMblock_release( struct inode *inode, struct file *file )
{
    L4VMblock_client_t *client = &L4VMblock_client;
    L4VMblock_descriptor_t *descriptor = 
	(L4VMblock_descriptor_t *)inode->i_bdev->bd_disk->private_data;

    if( descriptor->refcnt == 0 )
	return 0;
    descriptor->refcnt--;
    if( descriptor->refcnt )
	return 0;

    return L4VMblock_detach_device( client, descriptor );
}


static struct block_device_operations L4VMblock_device_operations = {
    .owner = THIS_MODULE,
    .open = L4VMblock_open,
    .release = L4VMblock_release,
};


static void 
L4VMblock_process_request( 
	L4VMblock_client_t *client, 
	request_queue_t *req_q, 
	struct request *req )
{
    struct bio *bio;
    struct bio_vec *bv;
    int i;
    L4VMblock_descriptor_t *descriptor = 
	(L4VMblock_descriptor_t *)req->rq_disk->private_data;
    volatile IVMblock_ring_descriptor_t *rdesc;
    L4VMblock_ring_t *ring_info = &client->desc_ring_info;
    sector_t sector;

    // Sanity check for connection to the block server.
    if( !descriptor->refcnt ) {
	dprintk( 2, KERN_ERR PREFIX "device not attached.\n" );
	req->errors++;
	return;
    }

    // The request must include at least one command.
    if( req->nr_hw_segments == 0 ) {
	req->errors++;
	return;
    }
    dprintk( 2, KERN_ERR PREFIX "request has %d segments\n", 
	    req->nr_hw_segments );

    rq_for_each_bio( bio, req )
    {
	sector = bio->bi_sector;

	bio_for_each_segment( bv, bio, i )
	{
	    L4VMblock_shadow_t *shadow;
	    L4_Word_t nsect;

	    // Wait for an available descriptor.
	    while( L4VMblock_ring_available(ring_info) == 0 )
	    {
		dprintk( 4, PREFIX "queue full, sleeping.\n" );
		spin_unlock_irq( req_q->queue_lock );
		L4VMblock_deliver_server_irq( client );
		wait_event( client->ring_wait,
			(L4VMblock_ring_available(ring_info) > 0) );
		spin_lock_irq( req_q->queue_lock );
		dprintk( 4, PREFIX "queue resumed.\n" );
	    }

	    dprintk( 2, PREFIX "block request %lx, size %d, "
		    "sector %llx, segments %d/%d/%d\n", 
		    bvec_to_phys(bv), bv->bv_len, sector,
		    bio->bi_hw_segments, bio->bi_phys_segments, bio->bi_vcnt );

	    // Get the next descriptor.
	    rdesc = &client->client_shared->desc_ring[ ring_info->start_free ];
	    ASSERT( !rdesc->status.X.server_owned );

	    // Get the shadow.
	    shadow = (L4VMblock_shadow_t *)rdesc->client_data;
	    shadow->bio = bio;
	    shadow->req = req;

	    // Fill-in the descriptor.
	    rdesc->handle = descriptor->handle;
	    rdesc->size = bv->bv_len;
	    rdesc->offset = sector;
	    rdesc->page = (void *)bvec_to_phys(bv);
	    rdesc->status.raw = 0;
	    rdesc->status.X.do_write = (rq_data_dir(req) == WRITE);
	    rdesc->status.X.speculative = (rq_data_dir(req) == READA);

	    // Update the request.
	    nsect = bv->bv_len >> 9;
	    sector += nsect;
	    process_that_request_first( req, nsect );

	    // Transfer the descriptor to the server.
	    rdesc->status.X.server_owned = 1;

	    // Next ...
	    ring_info->start_free = (ring_info->start_free+1) % ring_info->cnt;
	}
    }

    blkdev_dequeue_request( req );
}

static void
L4VMblock_end_request( L4VMblock_client_t *client, struct request *req )
    // Must be called while the queue lock is held.
{
    int uptodate = (req->errors == 0);

    if( !end_that_request_first(req, uptodate, req->nr_sectors) )
    {
	blkdev_dequeue_request( req );
	end_that_request_last(req);
    }
}

static void
L4VMblock_do_request( request_queue_t *q )
{
    struct request *req;
    L4VMblock_client_t *client = &L4VMblock_client;

    // Walk the requests.
    while( (req = elv_next_request(q)) != NULL )
    {
	req->errors = 0;
	L4VMblock_process_request( client, q, req );
	if( req->errors )
	    L4VMblock_end_request( client, req );
    }

    // Wake the block server.
    spin_unlock_irq( q->queue_lock );
    L4VMblock_deliver_server_irq( client );
    spin_lock_irq( q->queue_lock );
}


/****************************************************************************
 *
 * Functions for handling the server.
 *
 ****************************************************************************/

static void
L4VMblock_deliver_server_irq( L4VMblock_client_t *client )
{
    unsigned long flags;

    client->server_shared->irq_status |= L4VMBLOCK_SERVER_IRQ_DISPATCH;
    if( client->server_shared->irq_pending )
        return;

    client->server_shared->irq_pending = TRUE;

    dprintk(2, PREFIX "deliver server irq %d to %t\n",  
	    client->client_shared->server_irq_no, 
	    client->client_shared->server_irq_tid);

    local_irq_save( flags );
    l4ka_wedge_send_virtual_irq(client->client_shared->server_irq_no, 
				client->client_shared->server_irq_tid, L4_Never);
    local_irq_restore( flags );
    dprintk(2, PREFIX "deliver server irq done\n");    
}

static int L4VMblock_probe_device( 
	L4VMblock_client_t *client,
	kdev_t kdev,
	int minor )
{
    CORBA_Environment ipc_env;
    unsigned long irq_flags;
    IVMblock_devprobe_t probe_data;
    IVMblock_devid_t devid = { major: MAJOR(kdev), minor: MINOR(kdev) };
    struct gendisk *disk;

    dprintk( 1, PREFIX "probe remote device %d:%d tid %t\n", 
	     MAJOR(kdev), MINOR(kdev), client->server_tid );
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

    dprintk( 1, PREFIX "probed remote device %d:%d\n", 
	    MAJOR(kdev), MINOR(kdev) );
    dprintk( 2, PREFIX "device size: %lu KB\n"
	    "\tblock size: %lu\n"
	    "\thardware sector size: %lu\n"
	    "\tread ahead: %lu\n"
	    "\tmax read ahead: %lu\n"
	    "\trequest max sectors: %lu\n",
	    probe_data.device_size, probe_data.block_size,
	    probe_data.hardsect_size, probe_data.read_ahead,
	    probe_data.max_read_ahead, probe_data.req_max_sectors );

    L4VMblock_descriptors[ minor ].remote_kdev = kdev;
    L4VMblock_descriptors[ minor ].refcnt = 0;
    L4VMblock_descriptors[ minor ].lnx_disk = NULL;
    spin_lock_init( &L4VMblock_descriptors[ minor ].lnx_req_lock );

    disk = alloc_disk(1);
    if( disk == NULL )
	return -ENOMEM;

    disk->queue = blk_init_queue( L4VMblock_do_request, 
	    &L4VMblock_descriptors[minor].lnx_req_lock );
    if( NULL == disk->queue )
    {
	put_disk(disk);
	return -ENOMEM;
    }

    L4VMblock_descriptors[ minor ].lnx_disk = disk;

    disk->major = L4VMblock_major;
    disk->first_minor = minor;
    disk->fops = &L4VMblock_device_operations;
    disk->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO; // TODO: necessary?
    disk->private_data = (void *)&L4VMblock_descriptors[minor];
    sprintf( disk->disk_name, "vmblock%d", minor );
    sprintf( disk->devfs_name, "vmblock/%d", minor );
    // Convert from 1024-byte blocks to 512-byte blocks.
    set_capacity( disk, probe_data.device_size * 2 );
    add_disk( disk );

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

    dprintk( 2, PREFIX "opened %d:%d\n", 
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

    dprintk( 2, PREFIX "closed %d:%d\n", 
	    MAJOR(descriptor->remote_kdev), MINOR(descriptor->remote_kdev) );

    return 0;
}

static int L4VMblock_dspace_handler( 
	L4_Word_t fault_addr,
	L4_Word_t *ip,
	L4_Fpage_t *map_fp,
	void *data )
{
    // NOTE!!!!  This runs in the context of the L4 pager thread.  Do
    // not invoke Linux functions!!

    L4VMblock_client_t *client = &L4VMblock_client;
    L4_Word_t start = L4_Address(client->shared_window.fpage);
    L4_Word_t end = L4_Address(client->shared_window.fpage) + L4_Size(client->shared_window.fpage);
    idl4_fpage_t client_mapping, server_mapping;
    CORBA_Environment ipc_env;

    if( (fault_addr < start) || (fault_addr > end) )
	return FALSE;

    ipc_env = idl4_default_environment;
    idl4_set_rcv_window( &ipc_env, client->shared_window.fpage );
    IVMblock_Control_reattach( client->server_tid, client->handle,
	    &client_mapping, &server_mapping, &ipc_env );
    if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	while( 1 )
	    L4_KDB_Enter("dspace panic");
    }

    *map_fp = client->shared_window.fpage;
    L4_Set_Rights( map_fp, L4_FullyAccessible );

    return TRUE;
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
    unsigned long flags = 0;
    unsigned cleaned = 0;
    L4VMblock_shadow_t *shadow;
    struct request *req;
    struct bio *bio;
    L4_Word_t nsect;

    while( ring_info->start_dirty != ring_info->start_free )
    {
	// Get the next transaction.
	rdesc = &client->client_shared->desc_ring[ ring_info->start_dirty ];
	if( rdesc->status.X.server_owned )
	    break;

	cleaned++;

	// Get the shadow info.
	shadow = (L4VMblock_shadow_t *)rdesc->client_data;
	ASSERT( shadow );
	req = shadow->req;
	bio = shadow->bio;

	// Lock the request's queue.
	spin_lock_irqsave( req->q->queue_lock, flags );
	
	// End the bio.
	if( rdesc->status.X.server_err ) {
	    dprintk( 2, PREFIX "server block I/O error.\n" );
	    req->errors++;
	    bio_io_error( bio, rdesc->size );
	}
	else
	{
	    dprintk( 4, PREFIX "finished a bio.\n" );
	    bio_endio( bio, rdesc->size, 0 );
	}

	// End the request if it is finished.
	nsect = rdesc->size >> 9;
	if( req->hard_nr_sectors <= nsect )
	{
	    dprintk( 4, PREFIX "finished a request.\n" );
	    req->hard_nr_sectors = 0;
	    ASSERT( list_empty(&req->queuelist) );
	    end_that_request_last( req );
	}
	else
	    req->hard_nr_sectors -= nsect;

	// Unlock the request's queue.
	spin_unlock_irqrestore( req->q->queue_lock, flags );

	shadow->req = NULL;
	shadow->bio = NULL;
	// Move to the next.
	ring_info->start_dirty = (ring_info->start_dirty + 1) % ring_info->cnt;
    }

    if( cleaned )
	wake_up( &client->ring_wait );
}

static irqreturn_t
L4VMblock_irq_handler( int irq, void *data, struct pt_regs *regs )
{
    L4VMblock_client_t *client = (L4VMblock_client_t *)data;
    int cnt = 0;
    L4_Word_t events;

    do
    {
	client->client_shared->client_irq_pending = TRUE;
	events = irq_status_reset( &client->client_shared->client_irq_status );

	if( events )
	{
	    dprintk( 4, PREFIX "irq handler: 0x%lx\n", events );
	    L4VMblock_finish_transfers( client );
	}

	client->client_shared->client_irq_pending = FALSE;
	cnt++;
	if( cnt > 100 )
	    dprintk( 1, PREFIX "too many interrupts.\n" );
    }
    while( events );

    return IRQ_HANDLED;
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
    L4_Word_t index;

    // Try to allocate a virtual memory area for the shared windows.
    size  = L4_Size( L4_Fpage(0,sizeof(IVMblock_client_shared_t)) );
    size2 = L4_Size( L4_Fpage(0,sizeof(IVMblock_server_shared_t)) );
    log2size = L4_SizeLog2( L4_Fpage(0, size+size2) );
    err = L4VM_fpage_vmarea_get( log2size, &client->shared_window );
    if( err < 0 )
	return err;

    ASSERT( L4_Address(client->shared_window.fpage) >= 
	    (L4_Word_t)client->shared_window.ioremap_addr );

    dprintk( 2, PREFIX "receive window at %p, size %ld\n",
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
    dprintk( 2, PREFIX "client shared region at %p, offset %p, "
	    "size %ld\n",
	    (void *)idl4_fpage_get_base(client_mapping),
	    (void *)L4_Address(idl4_fpage_get_page(client_mapping)),
	    L4_Size(idl4_fpage_get_page(client_mapping)) );
    dprintk( 2, PREFIX "server shared region at %p, offset %p, "
	    "size %ld\n",
	    (void *)idl4_fpage_get_base(server_mapping),
	    (void *)L4_Address(idl4_fpage_get_page(server_mapping)),
	    L4_Size(idl4_fpage_get_page(server_mapping)) );

    window_base = L4_Address( client->shared_window.fpage );
    client->client_shared = (IVMblock_client_shared_t *)
	(window_base + idl4_fpage_get_base(client_mapping));
    client->server_shared = (IVMblock_server_shared_t *)
	(window_base + idl4_fpage_get_base(server_mapping));

    dprintk( 2, PREFIX "server irq tid: %p, server irq no: %lu\n",
	    RAW(client->client_shared->server_irq_tid),
	    client->client_shared->server_irq_no );

    // Initialize stuff related to the shared structures.
    client->desc_ring_info.cnt = IVMblock_descriptor_ring_size;
    client->desc_ring_info.start_free = 0;
    client->desc_ring_info.start_dirty = 0;
    init_waitqueue_head( &client->ring_wait );

    // Associate shadow info with each descriptor ring entry's client_data.
    // We learn about command reordering by changes in the client_data.
    for( index = 0; index < IVMblock_descriptor_ring_size; index++ )
	client->client_shared->desc_ring[ index ].client_data = 
	    (void *)&client->shadow_ring[ index ];

    return 0;
}

static int __init
L4VMblock_client_init_module( void )
{
    int i, err, minor;
    L4VMblock_client_t *client = &L4VMblock_client;

    
    if( L4VMblock_probe_devices[0] == 0 ) {
	// No block device requested; abort.
	return 0;
    }

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

    // Allocate a virtual interrupt.
    if( L4VMblock_irq > NR_IRQS )
    {
	printk( KERN_ERR PREFIX "unable to reserve a virtual interrupt.\n" );
	err = -ENOMEM;
	goto err_vmpic_reserve;
    }
    
    if (L4VMblock_irq == 0)
    {
#if defined(CONFIG_X86_IO_APIC)
        L4_KernelInterfacePage_t *kip = (L4_KernelInterfacePage_t *) L4_GetKernelInterface();
        L4VMblock_irq = L4_ThreadIdSystemBase(kip) + 6;	
	acpi_register_gsi(L4VMblock_irq, ACPI_LEVEL_SENSITIVE, ACPI_ACTIVE_LOW);
#else
	L4VMblock_irq = 7;
#endif
    }
    
    dprintk(2, PREFIX "client irq %d\n", L4VMblock_irq );
    l4ka_wedge_add_virtual_irq( L4VMblock_irq );
    err = request_irq( L4VMblock_irq, L4VMblock_irq_handler, 0, 
	    "vmblock", client );
    if( err < 0 )
    {
	printk( KERN_ERR PREFIX "unable to allocate an interrupt.\n" );
	goto err_request_irq;
    }
    client->client_shared->client_irq_no = L4VMblock_irq;
    client->client_shared->client_irq_tid = get_vcpu()->irq_gtid;
    client->client_shared->client_main_tid = get_vcpu()->main_gtid;

    // Register block handlers with Linux, and allocate a major number.
    err = register_blkdev( 0, "vmblock" );
    if( err < 0 )
	goto err_blkdev_register;
    else
	L4VMblock_major = err;

    // Finish the block driver init.

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

    l4ka_wedge_add_dspace_handler( L4VMblock_dspace_handler, NULL );
    
#if defined(CONFIG_AFTERBURN_DRIVERS_EARM_BLOCK_CLIENT)
    earm_disk_client_init();
#endif
    return 0;

err_blkdev_register:
    // TODO: free irq resources
err_request_irq:
err_vmpic_reserve:
    // TODO: disconnect from the block server
err_register:
err_no_server:
    return err;
}

static void __exit
L4VMblock_client_exit_module( void )
{
    // TODO: disconnect from the block server
}

MODULE_AUTHOR( "Joshua LeVasseur <jtl@irq.uka.de>" );
MODULE_DESCRIPTION( "L4Linux client stub block driver" );
MODULE_LICENSE( "Dual BSD/GPL" );
MODULE_SUPPORTED_DEVICE( "L4 VM block" );
MODULE_VERSION("slink");

module_init( L4VMblock_client_init_module );
module_exit( L4VMblock_client_exit_module ); 

#if defined(MODULE)
// Define a .afterburn_wedge_module section, to activate symbol resolution
// for this module against the wedge's symbols.
__attribute__ ((section(".afterburn_wedge_module")))
burn_wedge_header_t burn_wedge_header = {
    BURN_WEDGE_MODULE_INTERFACE_VERSION
};
#else

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

