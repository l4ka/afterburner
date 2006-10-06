/*********************************************************************
 *
 * Copyright (C) 2004-2006,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/net/client.h
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
#ifndef __L4KA_DRIVERS__NET__CLIENT_H__
#define __L4KA_DRIVERS__NET__CLIENT_H__

#include "L4VMnet_idl_client.h"
#include "net.h"

#define NR_RCV_GROUP	1

#define L4VMNET_IRQ_PKTS_AVAIL	1
#define L4VMNET_IRQ_RING_EMPTY	2

typedef struct
{
    int group_no;
    L4_ThreadId_t dev_tid;
    volatile unsigned status;
    volatile unsigned mask;
    L4VMnet_desc_ring_t rcv_ring;
    L4VMnet_shadow_t *rcv_shadow;
} L4VMnet_rcv_group_t;

typedef struct
{
    /* Data valid for an unopened device.
     */
    struct net_device *dev;
    int connected;
    int lanaddress_valid;

    /* Data valid after a connection is successfully opened to the server.
     */
    L4_ThreadId_t server_tid;	/* Server thread ID. */
    IVMnet_handle_t ivmnet_handle; /* Handle to the server. */
    int irq_pending;

    L4VM_alligned_vmarea_t shared_window;
    IVMnet_client_shared_t *client_shared;
    IVMnet_server_shared_t *server_status;

    L4VMnet_desc_ring_t send_ring;
    L4VMnet_shadow_t *send_shadow;

    L4VMnet_rcv_group_t rcv_group[NR_RCV_GROUP];
} L4VMnet_client_adapter_t;

#endif	/* __LINUXNET__L4VMNET_CLIENT_H__ */
