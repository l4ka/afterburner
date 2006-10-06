/*********************************************************************
 *                
 * Copyright (C) 2004 Joshua LeVasseur
 *
 * File path:	linuxblock/L4VMblock_client.h
 * Description:	Declarations specific to the block driver client.
 *
 * Proprietary!  DO NOT DISTRIBUTE!
 *
 * $Id: client.h,v 1.1 2006/09/21 09:28:35 joshua Exp $
 *                
 ********************************************************************/
#ifndef __LINUXBLOCK__L4VMBLOCK_CLIENT_H__
#define __LINUXBLOCK__L4VMBLOCK_CLIENT_H__

#include <linux/version.h>

#include "L4VMblock_idl_client.h"
#include "block.h"
#include <linuxglue/vmirq.h>
#include <linuxglue/vmserver.h>
#include <linuxglue/vmmemory.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
typedef dev_t kdev_t;
#endif

typedef struct
{
    kdev_t remote_kdev;
    IVMblock_handle_t handle;
    L4_Word_t refcnt;
} L4VMblock_descriptor_t;

struct L4VMblock_bh_shadow;
struct L4VMblock_reqest_shadow;

typedef struct L4VMblock_bh_shadow
{
    struct buffer_head *bh;
    struct L4VMblock_request_shadow *owner;
    struct list_head bh_list;
} L4VMblock_bh_shadow_t;

typedef struct L4VMblock_request_shadow
{
    struct request *request;
    unsigned bh_count;
    struct list_head bh_list;
    struct list_head shadow_list;
} L4VMblock_request_shadow_t;

typedef struct
{
    L4_ThreadId_t server_tid;
    IVMblock_handle_t handle;

    L4VM_alligned_vmarea_t shared_window;
    IVMblock_client_shared_t *client_shared;
    IVMblock_server_shared_t *server_shared;

    int rings_busy;
    L4VMblock_ring_t desc_ring_info;
    struct list_head shadow_list;
    wait_queue_head_t ring_wait;

} L4VMblock_client_t;

#endif	/* __LINUXBLOCK__L4VMBLOCK_CLIENT_H__ */
