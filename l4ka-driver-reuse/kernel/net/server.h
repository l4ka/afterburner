/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/net/server.h
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
#ifndef __L4KA_DRIVERS__NET__SERVER_H__
#define __L4KA_DRIVERS__NET__SERVER_H__

#include "L4VMnet_idl_server.h"
#include "L4VMnet_idl_reply.h"
#include "net.h"

#define L4VMNET_IRQ_BOTTOM_HALF_CMD	1
#define L4VMNET_IRQ_TOP_HALF_CMD	2
#define L4VMNET_IRQ_DISPATCH		4
#define L4VMNET_IRQ_DP83820_TX		8

// Forward declarations.
struct L4VMnet_server_cmd;
typedef struct L4VMnet_server_cmd L4VMnet_server_cmd_t;

typedef void (*L4VMnet_server_cmd_handler_t)( L4VMnet_server_cmd_t * );

struct L4VMnet_server_cmd
{
    volatile L4VMnet_server_cmd_handler_t handler;
    CORBA_Object reply_to_tid;

    union
    {
	struct {
	    char dev_name[IFNAMSIZ];
	    char lan_address[ETH_ALEN];
	} attach;

	struct {
	    IVMnet_handle_t handle;
	} reattach;

	struct {
	    IVMnet_handle_t handle;
	} detach;

	struct {
	    IVMnet_handle_t handle;
	} start;

	struct {
	    IVMnet_handle_t handle;
	} stop;

	struct {
	    IVMnet_handle_t handle;
	} update_stats;

	struct {
	    IVMnet_handle_t handle;
	    L4_Word_t client_paddr;
	    L4_Word_t ring_size_bytes;
	    L4_Word_t has_extended_status;
	} register_dp83820_tx_ring;

    } params;
};

#define L4VMNET_SERVER_CMD_RING_LEN	8

typedef struct L4VMnet_server_cmd_ring
{
    L4_Word16_t next_free;
    L4_Word16_t next_cmd;
    volatile int wake_server;
    L4VMnet_server_cmd_t cmds[L4VMNET_SERVER_CMD_RING_LEN];
} L4VMnet_server_cmd_ring_t;

typedef struct L4VMnet_client_info
{
    struct L4VMnet_client_info *next;
    L4VM_client_space_info_t *client_space;
    struct net_device *real_dev; /* Our partner Linux network device. */
    int operating;

    IVMnet_client_shared_t *shared_data;
    L4VMnet_desc_ring_t send_ring;
    L4VMnet_skb_ring_t rcv_ring; /* Packets to send to client. */

    struct {
	L4_Word_t first;
	L4_Word_t ring_start;
	L4VM_alligned_vmarea_t vmarea;
    } dp83820_tx;

    L4VM_alligned_alloc_t shared_alloc_info;
    IVMnet_handle_t ivmnet_handle;	/* Client's handle. */
    char lan_address[ETH_ALEN];
    char real_dev_name[IFNAMSIZ];
} L4VMnet_client_info_t;

typedef struct
{
    L4_ThreadId_t server_tid; /* Our server thread ID. */
    L4_ThreadId_t my_irq_tid; /* The L4Linux IRQ thread. */
    L4_ThreadId_t my_main_tid; /* The L4Linux main thread. */

    int irq_pending;
    volatile unsigned irq_status;
    volatile unsigned irq_mask;

    L4VMnet_server_cmd_ring_t top_half_cmds;
    L4VMnet_server_cmd_ring_t bottom_half_cmds;

    struct tq_struct bottom_half_task;

    IVMnet_server_shared_t *server_status;
    L4VM_alligned_alloc_t status_alloc_info;

    L4VMnet_client_info_t *client_list;
} L4VMnet_server_t;

extern L4VMnet_server_t L4VMnet_server;

#endif	/* __L4KA_DRIVERS__NET__SERVER_H__ */
