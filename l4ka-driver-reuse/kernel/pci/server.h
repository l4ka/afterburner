/*********************************************************************
 *
 * Copyright (C) 2004-2006,  University of Karlsruhe,  
 *                           University of New South Wales, Sydney
 *
 * File path:	l4ka-driver-reuse/kernel/pci/server.h
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
#ifndef __L4KA_DRIVER_REUSE__KERNEL__PCI__SERVER_H__
#define __L4KA_DRIVER_REUSE__KERNEL__PCI__SERVER_H__

#include <linux/list.h>
#include <linux/pci.h>

#include <l4/types.h>
#include <l4/kip.h>

#include <glue/vmserver.h>
#include <glue/bottomhalf.h>

#define PREFIX "L4VMpci server: "

#define L4VMPCI_IRQ_BOTTOM_HALF_CMD	(1)
#define L4VMPCI_IRQ_TOP_HALF_CMD	(2)


/**
 * Describes a PCI device managed by a client VM.
 */
typedef struct {
    /** node in the list of client-specific devices */
    struct list_head devs;
    /** the PCI device */
    struct pci_dev *dev;
    /** indicates whether a client may write to this device or not */
    int writable;
    /** the client's ID for this device */
    unsigned idx;
} L4VMpci_dev_t;

/**
 * Describes context associated with a client VM managing a specific
 * set of devices.
 */
typedef struct {
    /** node in the list of clients in a server context */
    struct list_head l_server;
    /** ID of the thread in the client VM */
    L4_ThreadId_t tid;
    /** The devices managed by this client VM */
    struct list_head devs;
} L4VMpci_client_t;

/**
 * Describes the parameters of a queued server command.
 */
typedef union
{
    struct {
	int dev_num;
	IVMpci_dev_id_t *devs;
    } reg;
    struct {
	L4_ThreadId_t tid;
    } dereg;
    struct {
	unsigned idx;
	unsigned reg;
	int len;
	int offset;
	unsigned int val;
    } r_w;
} L4VM_server_cmd_params_t;

/**
 * Describes context of a PCI config space server.
 */
typedef struct {
    
    L4_ThreadId_t server_tid; /* Our server thread ID. */
    L4_ThreadId_t my_irq_tid; /* The L4Linux IRQ thread. */
    L4_ThreadId_t my_main_tid; /* The L4Linux main thread. */

    int irq_pending;
    volatile unsigned irq_status;
    volatile unsigned irq_mask;

    struct list_head clients;

    struct tq_struct bottom_half_task;
    L4VM_server_cmd_ring_t top_half_cmds;
    L4VM_server_cmd_params_t top_half_params[L4VM_SERVER_CMD_RING_LEN];
    L4VM_server_cmd_ring_t bottom_half_cmds;
    L4VM_server_cmd_params_t bottom_half_params[L4VM_SERVER_CMD_RING_LEN];
} L4VMpci_server_t;


#endif	/* __L4KA_DRIVER_REUSE__KERNEL__PCI__SERVER_H__ */
