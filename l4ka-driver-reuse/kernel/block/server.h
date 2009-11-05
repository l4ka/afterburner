/*********************************************************************
 *                
 * Copyright (C) 2004, 2008-2009 Joshua LeVasseur
 *
 * File path:	block/server.h
 * Description:	Declarations for the Linux block driver server.
 *
 * Proprietary!  DO NOT DISTRIBUTE!
 *
 * $Id: server.h,v 1.1 2006/09/21 09:28:35 joshua Exp $
 *                
 ********************************************************************/
#ifndef __linuxblock__L4VMblock_server_h__
#define __linuxblock__L4VMblock_server_h__

#include <glue/thread.h>
#include <glue/bottomhalf.h>
#include <glue/vmmemory.h>
#include <glue/vmserver.h>
#include <glue/wedge.h>

#include "L4VMblock_idl_server.h"
#include "L4VMblock_idl_reply.h"
#include "block.h"

#define L4VMBLOCK_IRQ_BOTTOM_HALF_CMD	(1)
#define L4VMBLOCK_IRQ_TOP_HALF_CMD	(2)
#define L4VMBLOCK_IRQ_DISPATCH		(4)

#define L4VMBLOCK_MAX_DEVICES		(16)
#define L4VMBLOCK_MAX_CLIENTS		(8)

#define L4VMBLOCK_INVALID_HANDLE	(~0UL)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define L4VMBLOCK_DO_IRQ_DISPATCH
#endif

#undef PREFIX
#define PREFIX "L4VMblock server: "

typedef union
{
    struct {
	IVMblock_devid_t devid;
    } probe;

    struct {
	IVMblock_handle_t client_handle;
	IVMblock_devid_t devid;
	unsigned rw;
    } attach;

    struct {
	IVMblock_handle_t handle;
    } detach;

    struct {
	IVMblock_handle_t handle;
    } reattach;

} L4VM_server_cmd_params_t;


typedef struct
{
    IVMblock_handle_t handle;
    IVMblock_client_shared_t *client_shared;
    L4VM_alligned_alloc_t client_alloc_info;

    L4VMblock_ring_t ring_info;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    struct buffer_head bh_ring_storage[ IVMblock_descriptor_ring_size ];
    struct buffer_head *bh_ring[ IVMblock_descriptor_ring_size ];
#else
    struct bio *bio_ring[ IVMblock_descriptor_ring_size ];
#endif

    L4VM_client_space_info_t *client_space;
} L4VMblock_client_info_t;


typedef struct
{
    IVMblock_handle_t handle;
    L4VMblock_client_info_t *client;
    struct block_device *blkdev;
    L4_Word_t block_size;
    kdev_t kdev;
} L4VMblock_conn_info_t;


typedef struct L4VMblock_server
{
    L4_ThreadId_t server_tid; /* Our server thread ID. */
    L4_ThreadId_t my_irq_tid; /* The L4Linux IRQ thread. */
    L4_ThreadId_t my_main_tid; /* The L4Linux main thread. */

    int irq_pending;
    volatile unsigned irq_status;
    volatile unsigned irq_mask;
    
    struct tq_struct bottom_half_task;
#if !defined(L4VMBLOCK_DO_IRQ_DISPATCH)
    struct tq_struct dispatch_task;
#endif

    IVMblock_server_shared_t *server_info;
    L4VM_alligned_alloc_t server_alloc_info;

    L4VM_server_cmd_ring_t top_half_cmds;
    L4VM_server_cmd_params_t top_half_params[L4VM_SERVER_CMD_RING_LEN];
    L4VM_server_cmd_ring_t bottom_half_cmds;
    L4VM_server_cmd_params_t bottom_half_params[L4VM_SERVER_CMD_RING_LEN];

    L4VMblock_conn_info_t connections[L4VMBLOCK_MAX_DEVICES];
    L4VMblock_client_info_t clients[L4VMBLOCK_MAX_CLIENTS];

    spinlock_t ring_lock;

} L4VMblock_server_t;


extern inline L4VMblock_conn_info_t * L4VMblock_conn_lookup( 
	L4VMblock_server_t *server, IVMblock_handle_t handle )
{
    if( (handle < L4VMBLOCK_MAX_DEVICES) && 
	    (server->connections[handle].handle == handle) )
	return &server->connections[handle];
    return NULL;
}

extern inline void L4VMblock_conn_release( L4VMblock_conn_info_t *conn )
{
    conn->handle = L4VMBLOCK_INVALID_HANDLE;
}

extern inline L4VMblock_client_info_t * L4VMblock_client_lookup(
	L4VMblock_server_t *server, IVMblock_handle_t handle )
{
    if( (handle < L4VMBLOCK_MAX_CLIENTS) && 
	    (server->clients[handle].handle == handle) )
	return &server->clients[handle];
    return NULL;
}

extern inline void L4VMblock_client_release( L4VMblock_client_info_t *client )
{
    client->handle = L4VMBLOCK_INVALID_HANDLE;
}

extern void L4VMblock_process_client_io( L4VMblock_server_t *server );


#if defined(CONFIG_AFTERBURN_DRIVERS_EARM_BLOCK_SERVER)
#include <glue/earm.h>
#include "earm_idl_server.h"
#include "earm_idl_client.h"
#include "resourcemon_idl_client.h"

extern L4_Word_t L4VMblock_earm_bio_counter[L4VMBLOCK_MAX_CLIENTS];
extern L4_Word_t L4VMblock_earm_bio_budget[L4VMBLOCK_MAX_CLIENTS];
extern L4_Word_t L4VMblock_earm_bio_count, L4VMblock_earm_bio_size;
extern IEarm_shared_t *L4VM_earm_manager_shared;

extern int L4VMblock_earm_init(L4VMblock_server_t *server);
extern void L4VMblock_earm_end_io(L4_Word_t client_space_id, L4_Word_t size, 
				  L4_Word64_t *disk_energy, L4_Word64_t *cpu_energy);
extern L4_Word_t L4VMblock_earm_eas_get_throttle(L4_Word_t client_space_id);

#endif

#endif	/* __linuxblock__L4VMblock_h__ */
