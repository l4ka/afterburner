/*********************************************************************
 *                
 * Copyright (C) 2004 Joshua LeVasseur
 *
 * File path:	linuxblock/L4VMblock_client.h
 * Description:	Declarations specific to the block driver client.
 *
 * Proprietary!  DO NOT DISTRIBUTE!
 *
 * $Id: client26.h,v 1.1 2006/09/21 09:28:35 joshua Exp $
 *                
 ********************************************************************/
#ifndef __LINUXBLOCK__L4VMBLOCK_CLIENT_H__
#define __LINUXBLOCK__L4VMBLOCK_CLIENT_H__

#include <linux/version.h>
#include <linux/genhd.h>

#include "L4VMblock_idl_client.h"
#include "block.h"
#include <glue/vmirq.h>
#include <glue/vmserver.h>
#include <glue/vmmemory.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
typedef dev_t kdev_t;
#endif

typedef struct
{
    kdev_t remote_kdev;
    IVMblock_handle_t handle;
    L4_Word_t refcnt;
    struct gendisk *lnx_disk;
    spinlock_t lnx_req_lock;
} L4VMblock_descriptor_t;


typedef struct
{
    struct bio *bio;
    struct request *req;
} L4VMblock_shadow_t;

typedef struct
{
    L4_ThreadId_t server_tid;
    IVMblock_handle_t handle;

    L4VM_alligned_vmarea_t shared_window;
    IVMblock_client_shared_t *client_shared;
    IVMblock_server_shared_t *server_shared;

    L4VMblock_ring_t desc_ring_info;
    wait_queue_head_t ring_wait;
    L4VMblock_shadow_t shadow_ring[IVMblock_descriptor_ring_size];

} L4VMblock_client_t;

#endif	/* __LINUXBLOCK__L4VMBLOCK_CLIENT_H__ */
