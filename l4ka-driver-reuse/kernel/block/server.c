/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/block/server26.c
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

#define DO_L4VMBLOCK_REORDER
//#define L4VMBLOCK_READ_ONLY // Read-only can cause I/O errors (fs journals).

#include <l4/kip.h>

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
#include <linux/mm.h>

#include <asm/io.h>
#if defined(CONFIG_X86_IO_APIC)
#include <acpi/acpi.h>
#include <linux/acpi.h>
#endif

#include <linux/blkdev.h>
#include <linux/buffer_head.h>

#include "server.h"

#if !defined(CONFIG_AFTERBURN_DRIVERS_BLOCK_OPTIMIZE)
int L4VMblock_debug_level = 2;
MODULE_PARM( L4VMblock_debug_level, "i" );
#endif

int L4VMblock_server_irq = 0;
MODULE_PARM( L4VMblock_server_irq, "i" );
static L4VMblock_server_t L4VMblock_server;

static void L4VMblock_notify_tasklet_handler( unsigned long unused );
DECLARE_TASKLET( L4VMblock_notify_tasklet, L4VMblock_notify_tasklet_handler, 0);



static void L4VMblock_deliver_irq(L4_Word_t irq_flags)
{
    L4VM_server_deliver_irq(L4VMblock_server.my_irq_tid, L4VMblock_server_irq,  irq_flags,
			    &L4VMblock_server.irq_status, L4VMblock_server.irq_mask, 
			    &L4VMblock_server.irq_pending);
    
}

static L4_Word16_t L4VMblock_cmd_allocate_bottom(void)
{
    return L4VM_server_cmd_allocate( &L4VMblock_server.bottom_half_cmds, L4VMblock_server.my_irq_tid, 
				     L4VMblock_server_irq, L4VMBLOCK_IRQ_BOTTOM_HALF_CMD, 
				     &L4VMblock_server.irq_status, L4VMblock_server.irq_mask,
				     &L4VMblock_server.irq_pending);

}

/***************************************************************************
 *
 * Linux block functions.
 *
 ***************************************************************************/

static void
L4VMblock_deliver_client_irq( L4VMblock_client_info_t *client )
{
    unsigned long flags;
    L4_MsgTag_t tag;

    IVMblock_client_shared_t *shared = client->client_shared;
    
    client->client_shared->client_irq_status |= L4VMBLOCK_CLIENT_IRQ_CLEAN;
    if( client->client_shared->client_irq_pending )
	return;

    client->client_shared->client_irq_pending = TRUE;
	
    dprintk(2, KERN_INFO PREFIX "deliver client irq %d to %t\n",  
	    shared->client_irq_no, shared->client_irq_tid  );
    
    local_irq_save(flags);
    tag = l4ka_wedge_send_virtual_irq(shared->client_irq_no, shared->client_irq_tid, L4_Never);
    local_irq_restore(flags);
    
    
    if( L4_IpcFailed(tag) ) 
    {
	printk(PREFIX "delivering virq to client failed\n" );
	shared->client_irq_pending = 0;
    }
    dprintk(2, KERN_INFO PREFIX "deliver client irq done\n");    

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

static int L4VMblock_end_io(
	struct bio *bio,
	unsigned int bytes_done,
	int err
	)
{
    L4VMblock_server_t *server = &L4VMblock_server;
    IVMblock_ring_descriptor_t *desc;
    L4VMblock_conn_info_t *conn;
    L4_Word_t ring_index;
#if defined(CONFIG_AFTERBURN_DRIVERS_EARM_BLOCK)
    L4_Word64_t disk_energy, cpu_energy;
#endif

    spin_lock( &server->ring_lock );

    ASSERT(bio);
    desc = (IVMblock_ring_descriptor_t *)bio->bi_private;
    ASSERT(desc);

    if( err ) {
	desc->status.X.server_err = 1;
	dprintk(1, PREFIX "bio error\n" );
    }
	
    if( bio->bi_size ) {
	spin_unlock( &server->ring_lock );
	return 1;  // Not finished.
    }

    dprintk(2, PREFIX "io completed %lx/%p size %d va %x\n",
	    (L4_Word_t)bio->bi_sector, bio->bi_io_vec[0].bv_page, bio->bi_io_vec[0].bv_len,
	    (int *) pfn_to_kaddr(page_to_pfn(bio->bi_io_vec[0].bv_page)));	
    
    conn = L4VMblock_conn_lookup( server, desc->handle );
    
    if( !conn )
	dprintk(1, PREFIX "io completed, but connection is gone.\n");
    else
    {
#if defined(DO_L4VMBLOCK_REORDER)
 	L4VMblock_client_info_t *client = conn->client;
	L4VMblock_ring_t *ring_info = &client->ring_info;
	IVMblock_ring_descriptor_t *first_desc;
#endif
#if defined(CONFIG_AFTERBURN_DRIVERS_EARM_BLOCK)
	// Account energy
	L4VMblock_earm_end_io(client->client_space->space_id, 
			      desc->size, &disk_energy, &cpu_energy);
	desc->energy[3] = disk_energy;
	desc->energy[0] = cpu_energy;
	
#endif /* CONFIG_AFTERBURN_DRIVERS_EARM_BLOCK */

	ring_index = (L4_Word_t)(desc - conn->client->client_shared->desc_ring);

#if defined(DO_L4VMBLOCK_REORDER)
	first_desc = &client->client_shared->desc_ring[ ring_info->start_dirty];
	ASSERT( first_desc->status.X.server_owned );

	// Is it out-of-order?
	if( first_desc != desc )
	{
	    IVMblock_ring_descriptor_t tmp_desc;

	    // Swap the descriptor contents.
	    tmp_desc = *desc;
	    *desc = *first_desc;
	    *first_desc = tmp_desc;

	    // Update the relocated bio.
	    client->bio_ring[ ring_index ] = client->bio_ring[ ring_info->start_dirty ];
	    client->bio_ring[ ring_index ]->bi_private = desc;

	    // Update our desc pointer.
	    desc = first_desc;
	    ring_index = ring_info->start_dirty;

	    dprintk(2, PREFIX "out-of-order\n" );
	}

	ring_info->start_dirty = (ring_info->start_dirty + 1) % ring_info->cnt;
#endif

	// Release to the client.
	conn->client->bio_ring[ ring_index ] = NULL;
	desc->status.X.server_owned = 0;
    }

    spin_unlock( &server->ring_lock );

    bio->bi_private = NULL;
    bio_put( bio );

#if 1
    tasklet_schedule( &L4VMblock_notify_tasklet );
#else
    L4VMblock_deliver_client_irq( conn->client );
#endif

    return 0;
}

static int L4VMblock_initiate_io(
    L4VMblock_conn_info_t *conn,
    IVMblock_ring_descriptor_t *desc,
    L4_Word_t ring_index)
{
    // Inspired by submit_bh() in fs/buffer.c.
    IVMblock_client_shared_t *cs = conn->client->client_shared;
    struct bio *bio;
    L4_Word_t paddr;
    
    int rw,i;

    if(desc->size)
	bio = bio_alloc( GFP_NOIO, 1 );
    else
	bio = bio_alloc( GFP_NOIO, desc->count );

    ASSERT(bio);

    conn->client->bio_ring[ ring_index ] = bio;

    bio->bi_sector              = desc->offset;
    bio->bi_bdev                = conn->blkdev;
    
    if(desc->size) 
    {
	paddr = l4ka_wedge_bus_to_phys( (L4_Word_t)desc->page + conn->client->client_space->bus_start);
	bio->bi_io_vec[0].bv_page   = mem_map + (paddr >> PAGE_SHIFT);
	bio->bi_io_vec[0].bv_len    = desc->size;
	bio->bi_io_vec[0].bv_offset = paddr & ~(PAGE_MASK);

	bio->bi_vcnt = 1;
	bio->bi_idx  = 0;
	bio->bi_size = desc->size;
	
	dprintk(2, PREFIX "io submit sector %x single page %x mem %x cphys %x (start %x)  pa %x va %x size %u\n",
		(L4_Word_t)bio->bi_sector, bio->bi_io_vec[0].bv_page, mem_map, desc->page, 
		conn->client->client_space->bus_start, paddr, 
		pfn_to_kaddr(page_to_pfn(bio->bi_io_vec[0].bv_page)), desc->size);

	//ASSERT(virt_addr_valid(pfn_to_kaddr(page_to_pfn(bio->bi_io_vec[0].bv_page))));
    }
    else 
    { 
        // dma vectors used
	bio->bi_vcnt = desc->count;
	bio->bi_idx  = 0;
	bio->bi_size = 0;
	ASSERT( desc->count <= IVMblock_descriptor_max_vectors );
	
	for( i=0;i<desc->count;i++) 
	{
	    paddr = l4ka_wedge_bus_to_phys( (L4_Word_t)cs->dma_vec[i].page + conn->client->client_space->bus_start);
	    bio->bi_io_vec[i].bv_page = mem_map + (paddr >> PAGE_SHIFT);
	    bio->bi_io_vec[i].bv_len    = cs->dma_vec[i].size;
	    bio->bi_io_vec[i].bv_offset = paddr & ~(PAGE_MASK);
	    bio->bi_size += cs->dma_vec[i].size;

	    dprintk(2, PREFIX "io submit sector %x, vec %d/%d page %x mem %x cphys %x (start %x) pa %x va %x size %u\n",
		    (L4_Word_t)bio->bi_sector, i, bio->bi_vcnt, bio->bi_io_vec[i].bv_page, mem_map,
		    cs->dma_vec[i].page, conn->client->client_space->bus_start, paddr, 
		    pfn_to_kaddr(page_to_pfn(bio->bi_io_vec[i].bv_page)), bio->bi_size);
	    
	    ASSERT(virt_addr_valid(pfn_to_kaddr(page_to_pfn(bio->bi_io_vec[i].bv_page))));
	    
	}
    }

    bio->bi_end_io  = L4VMblock_end_io;
    bio->bi_private = desc;

#ifndef L4VMBLOCK_READ_ONLY
    if( desc->status.X.do_write )
	rw = WRITE;
    else if( desc->status.X.speculative )
	rw = READA;
    else
#endif
	rw = READ;

    submit_bio( rw, bio );

    return 1;
}

static int L4VMblock_process_io_queue(
    L4VMblock_server_t *server,
    L4VMblock_client_info_t *client,
    int client_idx)
{
    L4VMblock_ring_t *ring_info =  NULL;
    IVMblock_ring_descriptor_t *desc;
    L4VMblock_conn_info_t *conn;
    int errors = 0;
    L4_Word_t ring_index;
    
    ASSERT(server);
    ASSERT(client);
    ring_info = &client->ring_info;

#if defined(CONFIG_AFTERBURN_DRIVERS_EARM_BLOCK)
    if (L4VMblock_earm_bio_counter[client_idx] > L4VMblock_earm_bio_counter[client_idx])
    {
	dprintk( 4, KERN_INFO PREFIX "defer processing of client %d (throttle %d, ct %d)\n", 
		 (int) client_idx, 
		 (int) L4VMblock_earm_eas_get_throttle(client->client_space->space_id),
		 (int) L4VMblock_earm_bio_counter[client_idx]);
	
	return 1;
    }
    else
    {
    	L4VMblock_earm_bio_counter[client_idx]++;
    }
#endif

    // Manage the rings.
    spin_lock( &server->ring_lock );
    ring_index = ring_info->start_free;

    desc = &client->client_shared->desc_ring[ ring_index ];
    if( !desc->status.X.server_owned || client->bio_ring[ring_index] )
    {
	spin_unlock( &server->ring_lock );
	return 1;
    }
	
	
    ring_info->start_free = (ring_info->start_free + 1) % ring_info->cnt;
    spin_unlock( &server->ring_lock );
	
    // Start the I/O (if possible).
    conn = L4VMblock_conn_lookup( server, desc->handle );
    if( !conn || (conn->client != client) || 
	!L4VMblock_initiate_io(conn, desc, ring_index) )
    {
	desc->status.X.server_err = 1;
	desc->status.X.server_owned = 0;
	errors++;
    }
	
    if( errors )
	tasklet_schedule( &L4VMblock_notify_tasklet );

    return 0;
}

void L4VMblock_process_client_io( L4VMblock_server_t *server )
{
    int client_idx, done;
    L4VMblock_client_info_t *client[L4VMBLOCK_MAX_CLIENTS];
  
    do 
    {
	done = 0;
	for( client_idx = 0; client_idx < L4VMBLOCK_MAX_CLIENTS; client_idx++ )
	{
	    client[client_idx] = L4VMblock_client_lookup( server, client_idx );
	    if( client[client_idx] )
	    {
		dprintk( 4, KERN_INFO PREFIX "process client io %p (%d)\n", 
			 client[client_idx], 
			 (u32) client[client_idx]->client_space->space_id);

		done += L4VMblock_process_io_queue(server, client[client_idx], client_idx);
	    }
	    else
		done++;
	}
    } while (done < L4VMBLOCK_MAX_CLIENTS);
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
    unsigned long flags;

    kdev_t kdev;
    struct block_device *bdev;
    L4_Word_t minor = params->probe.devid.minor;
    L4_Word_t major = params->probe.devid.major;

    ASSERT( cmd->reply_to_tid.raw );
    ASSERT( params );
    ASSERT( server->server_tid.raw );
    ipc_env._action = 0;
    kdev = MKDEV( major, minor );

    dprintk(2, PREFIX "probe request from %p for device %u:%u\n", RAW(cmd->reply_to_tid), MAJOR(kdev), MINOR(kdev) );

    bdev = open_by_devnum( kdev, FMODE_READ );
    
    if( IS_ERR(bdev) || !bdev->bd_disk || !bdev->bd_disk->queue )
	CORBA_exception_set( &ipc_env, ex_IVMblock_invalid_device, NULL );
    else
    {
	dprintk( 2, PREFIX "\t dev %s (%s) id %u/%u\n", 
		 bdev->bd_disk->disk_name, 
		 ((bdev->bd_disk->flags & GENHD_FL_CD) ? "CD-ROM" : "HDD"), 
		 (u32) params->probe.devid.major, (u32) params->probe.devid.minor);
	
	probe_data.devid = params->probe.devid;
	probe_data.device_flags = bdev->bd_disk->flags;

	dprintk( 2, PREFIX "\t block_size %u\n", (int) block_size(bdev));
	probe_data.block_size = block_size(bdev);

	dprintk( 2, PREFIX "\t hardsect_size %u\n",  (int) queue_hardsect_size(bdev->bd_disk->queue));
	probe_data.hardsect_size = queue_hardsect_size(bdev->bd_disk->queue);
	
	probe_data.req_max_sectors = bdev->bd_disk->queue->max_sectors;
	dprintk( 2, PREFIX "\t max_sectors %u\n",  (int) bdev->bd_disk->queue->max_sectors);
	

	if(minor & ~0x40)
	{
	    if (bdev->bd_disk->flags & GENHD_FL_SUPPRESS_PARTITION_INFO)
	    {
		// Convert the partition size from 512-byte blocks to
		// 1024-byte blocks.
		dprintk( 2, PREFIX "\t device_size (capacity) %u\n", 
			 (int) (get_capacity(bdev->bd_disk) / 2));
		probe_data.device_size = get_capacity(bdev->bd_disk) / 2;
	    }
	    else
	    {
		L4_Word_t pm = minor - bdev->bd_disk->first_minor;
		// Convert the partition size from 512-byte blocks to 1024-byte blocks.
		dprintk( 2, PREFIX "\t device_size %u\n", 
			 (u32)bdev->bd_disk->part[pm - 1]->nr_sects / 2);
		probe_data.device_size = bdev->bd_disk->part[pm - 1]->nr_sects / 2 ;
	    }
	}
	else
	{
	    dprintk( 2, PREFIX "\t device_size %u\n",  (u32) bdev->bd_disk->capacity);
	    probe_data.device_size = bdev->bd_disk->capacity;
	}

	// Fudge this stuff.
	probe_data.read_ahead = 8;
	probe_data.max_read_ahead = 31;
    }

    if( !IS_ERR(bdev) )
	blkdev_put( bdev );

    local_irq_save(flags);
    L4_Set_VirtualSender( server->server_tid );
    IVMblock_Control_probe_propagate_reply( cmd->reply_to_tid,
					    &probe_data, &ipc_env );
    local_irq_restore(flags);
}

static int L4VMblock_allocate_conn_handle( 
	L4VMblock_server_t *server,
	IVMblock_handle_t *handle )
{
    static spinlock_t lock = SPIN_LOCK_UNLOCKED;

    spin_lock( &lock );
    {
	int i;
	for( i = 0; i < L4VMBLOCK_MAX_DEVICES; i++ )
	    if( server->connections[i].handle == L4VMBLOCK_INVALID_HANDLE )
	    {
		*handle = i;
		server->connections[i].handle = *handle;
		spin_unlock( &lock );
		return TRUE;
	    }
    }
    spin_unlock( &lock );

    return FALSE;
}

static int L4VMblock_allocate_client_handle(
	L4VMblock_server_t *server,
	IVMblock_handle_t *handle )
{
    static spinlock_t lock = SPIN_LOCK_UNLOCKED;

    spin_lock( &lock );
    {
	int i;
	for( i = 0; i < L4VMBLOCK_MAX_CLIENTS; i++ )
	    if( server->clients[i].handle == L4VMBLOCK_INVALID_HANDLE )
	    {
		*handle = i;
		server->clients[i].handle = *handle;
		spin_unlock( &lock );
		return TRUE;
	    }
    }
    spin_unlock( &lock );

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
    unsigned long flags;

    kdev_t kdev;
    struct block_device *bdev;
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

    bdev = open_by_devnum( kdev, FMODE_READ | FMODE_WRITE );
    if( IS_ERR(bdev) )
    {
	// Try to open device R/O
	bdev = open_by_devnum( kdev, FMODE_READ);
	if( IS_ERR(bdev) )
	{
	    CORBA_exception_set( &ipc_env, ex_IVMblock_invalid_device, NULL );
	    goto err_no_device;
	}
    }

    err = bd_claim( bdev, L4VMblock_attach_handler );
    if( err < 0 )
    {
	CORBA_exception_set( &ipc_env, ex_IVMblock_invalid_device, NULL );
	goto err_is_mounted;
    }

    err = set_blocksize( bdev, PAGE_SIZE );
    if( err < 0 )
    {
	CORBA_exception_set( &ipc_env, ex_IVMblock_invalid_device, NULL );
	goto err_blocksize;
    }

    if( !L4VMblock_allocate_conn_handle(server, &handle) )
    {
	CORBA_exception_set( &ipc_env, ex_IVMblock_no_memory, NULL );
	goto err_handle;
    }
    conn = L4VMblock_conn_lookup( server, handle );
    conn->blkdev = bdev;
    conn->kdev = kdev;
    conn->client = client;

    dprintk(1, PREFIX "opened device.\n" );

    local_irq_save(flags);
    L4_Set_VirtualSender( server->server_tid );
    IVMblock_Control_attach_propagate_reply( cmd->reply_to_tid,
	    &handle, &ipc_env );
    local_irq_restore(flags);
    return;

err_handle:
err_blocksize:
    bd_release( bdev );
err_is_mounted:
    blkdev_put( bdev );
err_no_device:
err_client:
    printk(PREFIX "failed to open a device.\n" );
    local_irq_save(flags);
    L4_Set_VirtualSender( server->server_tid );
    IVMblock_Control_attach_propagate_reply( cmd->reply_to_tid, NULL, &ipc_env);
    local_irq_restore(flags);
}

static void
L4VMblock_detach_handler( L4VM_server_cmd_t *cmd, void *data )
{ 
    L4VMblock_server_t *server = (L4VMblock_server_t *)data;
    idl4_server_environment ipc_env;
    L4VM_server_cmd_params_t *params = (L4VM_server_cmd_params_t *)cmd->data;
    L4VMblock_conn_info_t *conn;
    unsigned long flags;
 
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
    {
	bd_release( conn->blkdev );
	blkdev_put( conn->blkdev );
    }
    conn->blkdev = NULL;
    L4VMblock_conn_release( conn );

out:
    local_irq_save(flags);
    L4_Set_VirtualSender( server->server_tid );
    IVMblock_Control_detach_propagate_reply( cmd->reply_to_tid, &ipc_env );
    local_irq_restore(flags);
}

static void
L4VMblock_register_handler( L4VM_server_cmd_t *cmd, void *data )
{ 
    L4VMblock_server_t *server = (L4VMblock_server_t *)data;
    idl4_server_environment ipc_env;
    unsigned long flags;

    IVMblock_handle_t handle;
    L4VMblock_client_info_t *client;
    L4_Word_t log2size;
    int err, i;
    idl4_fpage_t idl4_fp_client, idl4_fp_server;

    ASSERT( cmd->reply_to_tid.raw );
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

    dprintk(1, PREFIX "allocated client shared region at %p, size %ld (%d)\n",
	    (void *)L4_Address(client->client_alloc_info.fpage),
	    L4_Size(client->client_alloc_info.fpage), 
	    sizeof(IVMblock_client_shared_t) );

    // Init the client shared page.
    client->client_shared = (IVMblock_client_shared_t *)
	L4_Address( client->client_alloc_info.fpage );
    
    client->client_shared->server_irq_no = L4VMblock_server_irq;
    client->client_shared->server_irq_tid = L4VMblock_server.my_irq_tid;
    client->client_shared->server_main_tid = L4VMblock_server.my_main_tid;
    
    dprintk(1, PREFIX "client shared server irq %d tid %t main %t\n",
            client->client_shared->server_irq_no,
            client->client_shared->server_irq_tid,
            client->client_shared->server_main_tid);
            
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
	client->bio_ring[i] = NULL;

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
    local_irq_save(flags);
    L4_Set_VirtualSender( server->server_tid );
    IVMblock_Control_register_propagate_reply( cmd->reply_to_tid, 
	    &handle, &idl4_fp_client, &idl4_fp_server, &ipc_env );
    local_irq_restore(flags);
    return;

err_shared_alloc:
    L4VMblock_client_release( client );
err_client_space:
err_handle:
    local_irq_save(flags);
    L4_Set_VirtualSender( server->server_tid );
    IVMblock_Control_register_propagate_reply( cmd->reply_to_tid, 
	    0, NULL, NULL, &ipc_env );
    local_irq_restore(flags);
}

static void
L4VMblock_reattach_handler( L4VM_server_cmd_t *cmd, void *data )
{ 
    L4VMblock_server_t *server = (L4VMblock_server_t *)data;
    L4VM_server_cmd_params_t *params = (L4VM_server_cmd_params_t *)cmd->data;
    L4VMblock_client_info_t *client;
    idl4_server_environment ipc_env;
    unsigned long flags;
    idl4_fpage_t idl4_fp_client, idl4_fp_server;

    ASSERT( cmd->reply_to_tid.raw );
    ASSERT( server->server_tid.raw );
    ipc_env._action = 0;

    // Lookup the client.
    client = L4VMblock_client_lookup( server, params->attach.client_handle );
    if( client == NULL )
    {
	CORBA_exception_set( &ipc_env, ex_IVMblock_invalid_client, NULL );
	goto err_client;
    }

    L4VM_page_in( client->client_alloc_info.fpage, PAGE_SIZE );
    L4VM_page_in( server->server_alloc_info.fpage, PAGE_SIZE );

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
    local_irq_save(flags);
    L4_Set_VirtualSender( server->server_tid );
    IVMblock_Control_reattach_propagate_reply( cmd->reply_to_tid, 
	    &idl4_fp_client, &idl4_fp_server, &ipc_env );
    local_irq_restore(flags);
    return;

err_client:
    local_irq_save(flags);
    L4_Set_VirtualSender( server->server_tid );
    IVMblock_Control_reattach_propagate_reply( cmd->reply_to_tid, 
	    NULL, NULL, &ipc_env );
    local_irq_restore(flags);
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

static irqreturn_t
L4VMblock_irq_handler( int irq, void *data, struct pt_regs *regs )
{
    L4VMblock_server_t *server = (L4VMblock_server_t *)data;

    while( 1 )
    {
	// Outstanding events? Read them and reset without losing events.
	L4_Word_t events = irq_status_reset( &server->irq_status );
	L4_Word_t client_events = irq_status_reset( (volatile unsigned *) &server->server_info->irq_status);
	
	if( !events && !client_events )
	    break;

	dprintk(2, PREFIX "irq handler: 0x%lx/0x%lx\n", events, client_events );

	server->irq_pending = TRUE;
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
	server->irq_pending = FALSE;
	server->server_info->irq_pending = FALSE;
    }

    return IRQ_HANDLED;
}

#if 0
static int
L4VMblock_irq_pending( void *data )
{
    L4VMblock_server_t *server = (L4VMblock_server_t *)data;

    return (server->irq_status & server->irq_mask) ||
	server->server_info->irq_status;
}
#endif

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

    dprintk(1, PREFIX "server thread: client probe request\n" );

    cmd_idx = L4VMblock_cmd_allocate_bottom();
   
    cmd = &server->bottom_half_cmds.cmds[ cmd_idx ];
    params = &server->bottom_half_params[ cmd_idx ];

    params->probe.devid = *devid;
    cmd->reply_to_tid = _caller;
    cmd->data = params;
    cmd->handler = L4VMblock_probe_handler;

    L4VMblock_deliver_irq(L4VMBLOCK_IRQ_BOTTOM_HALF_CMD);
    idl4_set_no_response( _env );  // Handle the request in a safe context.
}
IDL4_PUBLISH_ATTR
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

    dprintk(1, PREFIX "server thread: client attach request\n" );

    cmd_idx = L4VMblock_cmd_allocate_bottom();
    cmd = &server->bottom_half_cmds.cmds[ cmd_idx ];
    params = &server->bottom_half_params[ cmd_idx ];

    params->attach.client_handle = client_handle;
    params->attach.devid = *devid;
    params->attach.rw = rw;
    cmd->reply_to_tid = _caller;
    cmd->data = params;
    cmd->handler = L4VMblock_attach_handler;

    L4VMblock_deliver_irq(L4VMBLOCK_IRQ_BOTTOM_HALF_CMD);
    idl4_set_no_response( _env );  // Handle the request in a safe context.
}
IDL4_PUBLISH_ATTR
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

    dprintk(1, PREFIX "server thread: client detach request\n" );

    cmd_idx = L4VMblock_cmd_allocate_bottom();
    cmd = &server->bottom_half_cmds.cmds[ cmd_idx ];
    params = &server->bottom_half_params[ cmd_idx ];

    params->detach.handle = handle;
    cmd->reply_to_tid = _caller;
    cmd->data = params;
    cmd->handler = L4VMblock_detach_handler;

    L4VMblock_deliver_irq(L4VMBLOCK_IRQ_BOTTOM_HALF_CMD);
    idl4_set_no_response( _env );  // Handle the request in a safe context.
}
IDL4_PUBLISH_ATTR
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

    dprintk(1, PREFIX "server thread: client register request\n" );

    cmd_idx = L4VMblock_cmd_allocate_bottom();
    cmd = &server->bottom_half_cmds.cmds[ cmd_idx ];
    params = &server->bottom_half_params[ cmd_idx ];

    cmd->reply_to_tid = _caller;
    cmd->data = params;
    cmd->handler = L4VMblock_register_handler;

    L4VMblock_deliver_irq(L4VMBLOCK_IRQ_BOTTOM_HALF_CMD);

    idl4_set_no_response( _env );  // Handle the request in a safe context.
}
IDL4_PUBLISH_ATTR
IDL4_PUBLISH_IVMBLOCK_CONTROL_REGISTER(IVMblock_Control_register_implementation);


IDL4_INLINE void IVMblock_Control_reattach_implementation(
    CORBA_Object _caller,
    const IVMblock_handle_t client_handle,
    idl4_fpage_t *client_config,
    idl4_fpage_t *server_config,
    idl4_server_environment *_env)
{
    L4VMblock_server_t *server = &L4VMblock_server;
    L4_Word16_t cmd_idx;
    L4VM_server_cmd_t *cmd;
    L4VM_server_cmd_params_t *params;

    dprintk(1, PREFIX "server thread: client reattach request\n" );

    cmd_idx = L4VMblock_cmd_allocate_bottom();
    cmd = &server->bottom_half_cmds.cmds[ cmd_idx ];
    params = &server->bottom_half_params[ cmd_idx ];

    params->reattach.handle = client_handle;
    cmd->reply_to_tid = _caller;
    cmd->data = params;
    cmd->handler = L4VMblock_reattach_handler;

    L4VMblock_deliver_irq(L4VMBLOCK_IRQ_BOTTOM_HALF_CMD);

    idl4_set_no_response( _env );  // Handle the request in a safe context.
}
IDL4_PUBLISH_ATTR
IDL4_PUBLISH_IVMBLOCK_CONTROL_REATTACH(IVMblock_Control_reattach_implementation);



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
	    {
		break;
	    }

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
    
    spin_lock_init( &server->ring_lock );
    server->irq_status = 0;
    server->irq_mask = ~0;
    server->irq_pending = FALSE;
    server->my_irq_tid = get_vcpu()->irq_gtid;
    server->my_main_tid = get_vcpu()->main_gtid;

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

    // Allocate a virtual interrupt.
    if (L4VMblock_server_irq == 0)
    {
#if defined(CONFIG_X86_IO_APIC)
        L4_KernelInterfacePage_t *kip = (L4_KernelInterfacePage_t *) L4_GetKernelInterface();
        L4VMblock_server_irq = L4_ThreadIdSystemBase(kip) + 5;	
	acpi_register_gsi(L4VMblock_server_irq, ACPI_LEVEL_SENSITIVE, ACPI_ACTIVE_LOW);
#else
	L4VMblock_server_irq = 7;
#endif
    }
    
    dprintk(2, PREFIX "server irq %d\n", L4VMblock_server_irq );
    
    l4ka_wedge_add_virtual_irq( L4VMblock_server_irq );
    err = request_irq( L4VMblock_server_irq, L4VMblock_irq_handler, 0, "L4VMblock", server );
    if( err < 0 )
    {
	printk( KERN_ERR PREFIX "unable to allocate an interrupt.\n" );
	goto err_request_irq;
    }

    // Start the server thread.
    server->server_tid = L4VM_thread_create( GFP_KERNEL,
	    L4VMblock_server_thread, l4ka_wedge_get_irq_prio(), 
	    smp_processor_id(), &server, sizeof(server) );
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

#if defined(CONFIG_AFTERBURN_DRIVERS_EARM_BLOCK)
    // Start EARM
    L4VMblock_earm_init(server, L4VMblock_server_irq);
#endif
    printk(PREFIX "L4VMblock driver initialized.\n" );


    return 0;

err_thread_start:
    // TODO: free the interrupt handler
err_request_irq:
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

MODULE_AUTHOR( "Joshua LeVasseur <jtl@ira.uka.de>" );
MODULE_DESCRIPTION( "L4 block driver server" );
MODULE_LICENSE( "Dual BSD/GPL" );
MODULE_SUPPORTED_DEVICE( "L4 VM block" );
MODULE_VERSION( "alpha" );

module_init( L4VMblock_server_init_module );
module_exit( L4VMblock_server_exit_module );

#if defined(MODULE)
// Define a .afterburn_wedge_module section, to activate symbol resolution
// for this module against the wedge's symbols.
__attribute__ ((section(".afterburn_wedge_module")))
burn_wedge_header_t burn_wedge_header = {
    BURN_WEDGE_MODULE_INTERFACE_VERSION
};
#endif


